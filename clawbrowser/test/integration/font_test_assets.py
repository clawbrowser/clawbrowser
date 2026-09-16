"""Resolve test font bytes only inside the selected ClawBrowser installation."""
from pathlib import Path


def bundled_font_asset(binary, platform, catalog_id, name, resource_dir=None):
    binary = Path(binary).resolve()
    installation = binary.parents[1] if platform == 'darwin' else binary.parent
    root = Path(resource_dir).resolve() if resource_dir else installation
    if not root.is_relative_to(installation):
        raise ValueError('Font resource directory is outside the selected browser installation')
    if resource_dir and platform == 'win32' and not (root / 'chrome.dll').is_file():
        raise ValueError('Windows font resource directory must contain chrome.dll')
    relative = Path('clawbrowser-fonts') / catalog_id / 'fonts' / name
    matches = {path.resolve() for path in root.glob('**/' + relative.as_posix())
               if path.is_file()}
    if any(not path.is_relative_to(installation) for path in matches):
        raise ValueError('Bundled font resolves outside the selected installation')
    if len(matches) != 1:
        raise ValueError('Expected one bundled font asset; for multiple installed Windows '
                         'versions set CLAWBROWSER_TEST_RESOURCE_DIR to the tested module directory')
    asset = matches.pop()
    if platform == 'win32' and not (asset.parents[3] / 'chrome.dll').is_file():
        raise ValueError('Windows font catalog must be adjacent to chrome.dll')
    return asset
