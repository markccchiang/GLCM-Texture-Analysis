#!/usr/bin/env python3
"""Downloads the medical sample images and converts them into samples/medical/.

    python3 -m venv .venv-medical
    .venv-medical/bin/pip install pydicom numpy nibabel pillow
    .venv-medical/bin/python scripts/fetch-medical-samples.py

Each image is one 2D slice or radiograph, stored as a lossless 16-bit grayscale PNG. The sources, licenses and
conversions are described in samples/README.md. The script pins the exact series and instances, so running it again
gives the same pixels.
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


def save_png(pixels: np.ndarray, name: str) -> None:
    assert pixels.dtype == np.uint16
    path = OUTPUT / name
    Image.fromarray(pixels).save(path, optimize=True)
    digest = hashlib.sha256(pixels.tobytes()).hexdigest()
    print(f'{path.relative_to(ROOT)}: {pixels.shape[1]} × {pixels.shape[0]}, values {pixels.min()}–{pixels.max()}, pixels sha256 {digest}')


def ct_chest() -> None:
    dataset = dicom_instance(CT_SERIES, CT_INSTANCE)
    hounsfield = dataset.pixel_array.astype(np.int32) * int(dataset.RescaleSlope) + int(dataset.RescaleIntercept)
    # Stored value = HU + 1024: air is about 0, water 1024; the area outside the scan field (-2048 HU) becomes 0
    save_png(np.clip(hounsfield + 1024, 0, 4095).astype(np.uint16), 'ct-chest.png')


def xray_chest() -> None:
    dataset = dicom_instance(XRAY_SERIES, XRAY_INSTANCE)
    assert dataset.PhotometricInterpretation == 'MONOCHROME2'
    pixels = dataset.pixel_array.astype(np.float32)
    height = round(pixels.shape[0] * XRAY_WIDTH / pixels.shape[1])
    # Area averaging keeps the 15-bit values of the detector
    reduced = np.asarray(Image.fromarray(pixels, mode='F').resize((XRAY_WIDTH, height), Image.Resampling.BOX))
    save_png(np.clip(np.rint(reduced), 0, 65535).astype(np.uint16), 'xray-chest.png')


def mri_brain() -> None:
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'T1w.nii.gz'
        path.write_bytes(download(MRI_URL))
        volume = nib.as_closest_canonical(nib.load(path))
        data = np.asarray(volume.dataobj)
        # Axial slice with anterior at the top and the patient's right on the image's left
        axial = np.rot90(data[:, :, MRI_SLICE]).astype(np.int32)
        save_png(np.clip(axial, 0, 65535).astype(np.uint16), 'mri-brain-t1.png')


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    ct_chest()
    xray_chest()
    mri_brain()


if __name__ == '__main__':
    main()
