#!/usr/bin/env python3
"""Downloads the medical sample images and converts them into samples/medical/.

    python3 -m venv .venv-medical
    .venv-medical/bin/pip install -r scripts/requirements-medical.txt
    .venv-medical/bin/python scripts/fetch-medical-samples.py

Each image is one 2D slice or radiograph, stored as a lossless 16-bit grayscale PNG. The sources, licenses and
conversions are described in samples/README.md. The script pins the exact series and instances, the Python packages
are pinned in requirements-medical.txt, and the SHA-256 of every image's pixels is checked, so a changed download or
decoder stops the script instead of silently changing the samples.
"""

import hashlib
import io
import tempfile
import urllib.request
import zipfile
from pathlib import Path

import nibabel as nib
import numpy as np
import pydicom
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / 'samples' / 'medical'
NBIA = 'https://services.cancerimagingarchive.net/nbia-api/services/v1'

# LIDC-IDRI, patient LIDC-IDRI-0001: chest CT, the axial slice at z = -115 mm (lungs, heart, a nodule in the left lung)
CT_SERIES = '1.3.6.1.4.1.14519.5.2.1.6279.6001.179049373636438705059720603192'
CT_INSTANCE = '1.3.6.1.4.1.14519.5.2.1.6279.6001.297813206491522913194774892711'

# COVID-19-AR, patient COVID-19-AR-16406496: PA chest radiograph
XRAY_SERIES = '1.3.6.1.4.1.14519.5.2.1.9999.103.2033282158389577844152198164874'
XRAY_INSTANCE = '1.3.6.1.4.1.14519.5.2.1.9999.103.2294547012929720947691791872212'
XRAY_WIDTH = 1024

# Pancreas-CT, patient PANCREAS_0080: contrast-enhanced abdominal CT, the axial slice at z = -100 mm (liver, gallbladder,
# both kidneys, aorta, spine and bowel)
ABDOMEN_SERIES = '1.2.826.0.1.3680043.2.1125.1.41202274843063370955090296887703130'
ABDOMEN_INSTANCE = '1.2.826.0.1.3680043.2.1125.1.3951614841645739382916216145849403'

# CBIS-DDSM, Mass-Training_P_00001_LEFT_CC: digitized film mammogram, craniocaudal view of the left breast with a mass
MAMMOGRAM_SERIES = '1.3.6.1.4.1.9590.100.1.2.342386194811267636608694132590482924515'
MAMMOGRAM_INSTANCE = '1.3.6.1.4.1.9590.100.1.2.156556873010981646517128874312129349516'
MAMMOGRAM_WIDTH = 1024

# SHA-256 of the uint16 pixel array of each output file (row-major, native byte order as written by numpy)
EXPECTED_PIXELS_SHA256 = {
    'ct-chest.png': '6fc2fc92db49d2a769b629274c1e16d153cc2da0963229664ab1886ad811fd05',
    'xray-chest.png': '9b53b9fce54e7a16423b107776acb2d6a8ce3284e268b5dd61624c36c6bb91a8',
    'mri-brain-t1.png': 'be3ae4e4a35cbd3c99e228c4b0d851d6ac50cab77a23bfa5c3e7231232d2a47f',
    'ct-abdomen.png': 'aeae3fd51f7b3f1c704e45fdcdc3af0ef99c4fd91a6402c4d3decf169d6cabc7',
    'mammogram-cc.png': '3af6b82f57a8563dc477e8cc877b0a438d61e14d2b98d415c2f73e2c9db5d224',
}

# OpenNeuro ds000001, sub-01: T1-weighted anatomical MRI, axial slice 120 in RAS orientation
MRI_URL = 'https://s3.amazonaws.com/openneuro.org/ds000001/sub-01/anat/sub-01_T1w.nii.gz'
MRI_SLICE = 120


def download(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=600) as response:
        return response.read()


def dicom_instance(series_uid: str, instance_uid: str) -> pydicom.Dataset:
    """Downloads a series from TCIA and returns one instance of it"""
    archive = zipfile.ZipFile(io.BytesIO(download(f'{NBIA}/getImage?SeriesInstanceUID={series_uid}')))
    for name in archive.namelist():
        if name.endswith('.dcm'):
            dataset = pydicom.dcmread(io.BytesIO(archive.read(name)))
            if dataset.SOPInstanceUID == instance_uid:
                return dataset
    raise RuntimeError(f'Instance {instance_uid} not found in series {series_uid}')


def save_png(pixels: np.ndarray, name: str, spacing_mm: tuple[float, float] | None) -> None:
    """Writes a 16-bit PNG; spacing_mm (x, y) becomes its pHYs chunk, which stores whole pixels per metre (None: no pHYs)"""
    assert pixels.dtype == np.uint16
    digest = hashlib.sha256(pixels.astype('<u2').tobytes()).hexdigest()
    if digest != EXPECTED_PIXELS_SHA256[name]:
        raise RuntimeError(
            f'{name}: the pixels differ from the committed sample (sha256 {digest}, expected {EXPECTED_PIXELS_SHA256[name]}). '
            'The source data or a package version changed; check it before updating EXPECTED_PIXELS_SHA256.'
        )
    path = OUTPUT / name
    if spacing_mm is None:
        Image.fromarray(pixels).save(path, optimize=True)
    else:
        Image.fromarray(pixels).save(path, optimize=True, dpi=tuple(25.4 / mm for mm in spacing_mm))
    spacing = 'no pixel spacing' if spacing_mm is None else f'pixel spacing {spacing_mm[0]:.6g} × {spacing_mm[1]:.6g} mm'
    print(f'{path.relative_to(ROOT)}: {pixels.shape[1]} × {pixels.shape[0]}, values {pixels.min()}–{pixels.max()}, {spacing}, pixels sha256 {digest}')


def dicom_spacing(dataset: pydicom.Dataset) -> tuple[float, float]:
    """(x, y) in mm; DICOM lists the row spacing (between rows, y) first"""
    rows, columns = dataset.get('PixelSpacing') or dataset.ImagerPixelSpacing
    return float(columns), float(rows)


def ct_chest() -> None:
    dataset = dicom_instance(CT_SERIES, CT_INSTANCE)
    hounsfield = dataset.pixel_array.astype(np.int32) * int(dataset.RescaleSlope) + int(dataset.RescaleIntercept)
    # Stored value = HU + 1024: air is about 0, water 1024; the area outside the scan field (-2048 HU) becomes 0
    save_png(np.clip(hounsfield + 1024, 0, 4095).astype(np.uint16), 'ct-chest.png', dicom_spacing(dataset))


def ct_abdomen() -> None:
    dataset = dicom_instance(ABDOMEN_SERIES, ABDOMEN_INSTANCE)
    hounsfield = dataset.pixel_array.astype(np.int32) * int(dataset.RescaleSlope) + int(dataset.RescaleIntercept)
    # The column direction of this series points anterior (0, -1, 0), so the rows run from the back to the front; flip
    # them to show anterior at the top, as ct-chest.png
    column_direction = [float(value) for value in dataset.ImageOrientationPatient[3:]]
    assert column_direction == [0.0, -1.0, 0.0], column_direction
    hounsfield = np.flipud(hounsfield)
    save_png(np.clip(hounsfield + 1024, 0, 4095).astype(np.uint16), 'ct-abdomen.png', dicom_spacing(dataset))


def mammogram_cc() -> None:
    dataset = dicom_instance(MAMMOGRAM_SERIES, MAMMOGRAM_INSTANCE)
    assert dataset.PhotometricInterpretation == 'MONOCHROME2'
    pixels = dataset.pixel_array.astype(np.float32)
    height = round(pixels.shape[0] * MAMMOGRAM_WIDTH / pixels.shape[1])
    reduced = np.asarray(Image.fromarray(pixels).resize((MAMMOGRAM_WIDTH, height), Image.Resampling.BOX))
    # The digitized films carry no pixel spacing (CBIS-DDSM has none in its DICOM files), so the PNG has none either
    save_png(np.clip(np.rint(reduced), 0, 65535).astype(np.uint16), 'mammogram-cc.png', None)


def xray_chest() -> None:
    dataset = dicom_instance(XRAY_SERIES, XRAY_INSTANCE)
    assert dataset.PhotometricInterpretation == 'MONOCHROME2'
    pixels = dataset.pixel_array.astype(np.float32)
    height = round(pixels.shape[0] * XRAY_WIDTH / pixels.shape[1])
    # Area averaging keeps the 15-bit values of the detector
    # A float32 array becomes a mode "F" image (the mode argument of fromarray is deprecated)
    reduced = np.asarray(Image.fromarray(pixels).resize((XRAY_WIDTH, height), Image.Resampling.BOX))
    x_mm, y_mm = dicom_spacing(dataset)
    # Each output pixel covers original pixels in proportion to the reduction on its axis
    spacing = (x_mm * pixels.shape[1] / XRAY_WIDTH, y_mm * pixels.shape[0] / height)
    save_png(np.clip(np.rint(reduced), 0, 65535).astype(np.uint16), 'xray-chest.png', spacing)


def mri_brain() -> None:
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'T1w.nii.gz'
        path.write_bytes(download(MRI_URL))
        volume = nib.as_closest_canonical(nib.load(path))
        data = np.asarray(volume.dataobj)
        # Axial slice with anterior at the top and the patient's right on the image's left
        axial = np.rot90(data[:, :, MRI_SLICE]).astype(np.int32)
        # After rot90 the image columns run along the volume's first axis (x) and the rows along its second (y)
        zooms = volume.header.get_zooms()
        save_png(np.clip(axial, 0, 65535).astype(np.uint16), 'mri-brain-t1.png', (float(zooms[0]), float(zooms[1])))


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    ct_chest()
    ct_abdomen()
    xray_chest()
    mammogram_cc()
    mri_brain()


if __name__ == '__main__':
    main()
