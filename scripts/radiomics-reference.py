#!/usr/bin/env python3
"""Reference values of first-order statistics, run length and size zone features from PyRadiomics, for the core tests.

Writes core/tests/data/pyradiomics-firstorder.json (FirstOrderTest), pyradiomics-glrlm.json (RunLengthTest) and
pyradiomics-glszm.json (SizeZoneTest): for rectangle ROIs on sample images, the values of PyRadiomics' first-order,
GLRLM and GLSZM feature classes. Rectangles avoid differences in mask rasterization (the core covers the pixels whose
centres lie inside). Entropy and uniformity are computed with a bin width of 1 on 8-bit images, which matches the core
with quantization "none" and 256 gray levels; 16-bit cases only list the features that use the original intensities.
GLRLM and GLSZM use 8-bit images with a bin width of 1 in 2D (four in-plane directions, 8-connected zones), matching
the core's fixed bin width of 1.

Only developers run this script, to regenerate the reference data; the application and the tests never call Python.

Setup (Python 3.12; PyRadiomics 3.1.0 has no wheels for newer Python versions and is built from its git tag):
    uv venv --python 3.12 .venv-radiomics
    uv pip install --python .venv-radiomics/bin/python -r scripts/requirements-radiomics.txt
    .venv-radiomics/bin/python scripts/radiomics-reference.py
"""

import json
from pathlib import Path

import numpy as np
import radiomics
import SimpleITK as sitk
from radiomics import firstorder, glrlm, glszm

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / 'core' / 'tests' / 'data' / 'pyradiomics-firstorder.json'
GLRLM_OUTPUT = ROOT / 'core' / 'tests' / 'data' / 'pyradiomics-glrlm.json'
GLSZM_OUTPUT = ROOT / 'core' / 'tests' / 'data' / 'pyradiomics-glszm.json'

# PyRadiomics feature name -> core feature id
FEATURES = {
    'Minimum': 'Minimum',
    'Maximum': 'Maximum',
    'Range': 'Range',
    'Median': 'Median',
    '10Percentile': 'Percentile10',
    '90Percentile': 'Percentile90',
    'InterquartileRange': 'InterquartileRange',
    'MeanAbsoluteDeviation': 'MeanAbsoluteDeviation',
    'RobustMeanAbsoluteDeviation': 'RobustMeanAbsoluteDeviation',
    'RootMeanSquared': 'RootMeanSquared',
    'Energy': 'FirstOrderEnergy',
    'Variance': 'Variance',
    'Skewness': 'Skewness',
    'Kurtosis': 'Kurtosis',
    'Mean': 'Mean',
    'Entropy': 'FirstOrderEntropy',
    'Uniformity': 'Uniformity',
}
BINNED = {'Entropy', 'Uniformity'}

# Sample image, rectangle (x, y, width, height) in pixels
CASES = [
    ('textures/camera.png', (100, 100, 64, 48)),
    ('textures/brick.png', (200, 150, 90, 70)),
    ('medical/ct-chest.png', (180, 200, 120, 80)),
]
# Texture features need gray levels a bin width of 1 can hold within the core's 256 levels
EIGHT_BIT_CASES = [case for case in CASES if case[0].startswith('textures/')]


def reference(image_path: str, rectangle: tuple[int, int, int, int]) -> dict:
    pixels = sitk.GetArrayFromImage(sitk.ReadImage(str(ROOT / 'samples' / image_path)))
    assert pixels.ndim == 2, image_path
    eight_bit = pixels.dtype == np.uint8
    x, y, width, height = rectangle
    mask = np.zeros(pixels.shape, dtype=np.uint8)
    mask[y : y + height, x : x + width] = 1

    image = sitk.GetImageFromArray(pixels[np.newaxis])
    label = sitk.GetImageFromArray(mask[np.newaxis])
    features = firstorder.RadiomicsFirstOrder(image, label, binWidth=1, voxelArrayShift=0)
    features.enableAllFeatures()
    values = features.execute()
    return {
        'image': image_path,
        'rectangle': list(rectangle),
        'bitDepth': 8 if eight_bit else 16,
        'features': {
            core: float(values[name]) for name, core in FEATURES.items() if eight_bit or name not in BINNED
        },
    }


def texture_reference(feature_class, image_path: str, rectangle: tuple[int, int, int, int]) -> dict:
    """PyRadiomics values of a texture feature class in 2D with a bin width of 1, on an 8-bit image"""
    pixels = sitk.GetArrayFromImage(sitk.ReadImage(str(ROOT / 'samples' / image_path)))
    assert pixels.dtype == np.uint8, image_path
    x, y, width, height = rectangle
    mask = np.zeros(pixels.shape, dtype=np.uint8)
    mask[y : y + height, x : x + width] = 1
    image = sitk.GetImageFromArray(pixels[np.newaxis])
    label = sitk.GetImageFromArray(mask[np.newaxis])
    features = feature_class(image, label, binWidth=1, force2D=True, force2Ddimension=0)
    features.enableAllFeatures()
    values = features.execute()
    # Feature names without the core's prefix ("Glrlm", "Glszm")
    return {
        'image': image_path,
        'rectangle': list(rectangle),
        'features': {name: float(value) for name, value in sorted(values.items())},
    }


def main() -> None:
    document = {
        'source': (
            f'PyRadiomics {radiomics.__version__}, NumPy {np.__version__}, '
            f'SimpleITK {sitk.Version_VersionString()}'
        ),
        'settings': {'binWidth': 1, 'voxelArrayShift': 0, 'logBase': 'log2'},
        'cases': [reference(image, rectangle) for image, rectangle in CASES],
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(document, indent=2) + '\n')
    print(f'{OUTPUT.relative_to(ROOT)}: {len(document["cases"])} cases from {document["source"]}')

    run_lengths = {
        'source': document['source'],
        'settings': {'binWidth': 1, 'force2D': True, 'force2Ddimension': 0, 'logBase': 'log2'},
        'cases': [texture_reference(glrlm.RadiomicsGLRLM, image, rectangle) for image, rectangle in EIGHT_BIT_CASES],
    }
    GLRLM_OUTPUT.write_text(json.dumps(run_lengths, indent=2) + '\n')
    print(f'{GLRLM_OUTPUT.relative_to(ROOT)}: {len(run_lengths["cases"])} cases')

    size_zones = {
        'source': document['source'],
        'settings': {'binWidth': 1, 'force2D': True, 'force2Ddimension': 0, 'logBase': 'log2'},
        'cases': [texture_reference(glszm.RadiomicsGLSZM, image, rectangle) for image, rectangle in EIGHT_BIT_CASES],
    }
    GLSZM_OUTPUT.write_text(json.dumps(size_zones, indent=2) + '\n')
    print(f'{GLSZM_OUTPUT.relative_to(ROOT)}: {len(size_zones["cases"])} cases')


if __name__ == '__main__':
    main()
