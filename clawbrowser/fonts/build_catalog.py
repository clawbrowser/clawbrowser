"""GN adapter for versioned, checksum-verified font catalog assets."""
import argparse
import hashlib
import json
from pathlib import Path

from stage_fonts import stage


def build(manifest, chromium, destination, header):
    raw=json.loads(manifest.read_text())
    canonical=json.dumps(raw,indent=2)
    digest=hashlib.sha256(canonical.encode()).hexdigest()
    if destination.exists():
        # A versioned output is immutable. A changed manifest needs a new ID;
        # corrupt or incomplete output must never be silently accepted.
        if (destination/'manifest.json').read_text()!=canonical:
            raise ValueError('existing catalog differs; bump catalog_id')
        if (destination/'STAGED').read_text()!=raw['catalog_id']+'\n'+digest+'\n':
            raise ValueError('incomplete catalog staging marker')
        notices={'UPSTREAM-LICENSE':chromium/'third_party/test_fonts/LICENSE',
                 'NOTO-LICENSE':manifest.parent/'font-vendor-prototype/NOTO-LICENSE'}
        for name,source in notices.items():
            if (destination/name).read_bytes()!=source.read_bytes():
                raise ValueError('missing or changed font notice: '+name)
        expected={entry['file'] for entry in raw['fonts']}
        if {path.name for path in (destination/'fonts').iterdir()}!=expected:
            raise ValueError('unexpected catalog files')
        for entry in raw['fonts']:
            path=destination/'fonts'/entry['file']
            if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:
                raise ValueError('corrupt staged font: '+entry['file'])
    else:
        destination.parent.mkdir(parents=True,exist_ok=True)
        stage(manifest,chromium,destination)
    text=('#ifndef CLAWBROWSER_FONT_CATALOG_BUILD_H_\n'
          '#define CLAWBROWSER_FONT_CATALOG_BUILD_H_\n'
          'namespace clawbrowser {\n'
          'inline constexpr char kFontCatalogManifestHash[] = "'+digest+'";\n'
          'inline constexpr char kFontCatalogDirectory[] = "clawbrowser-fonts/'+raw['catalog_id']+'";\n'
          '}\n#endif\n')
    header.parent.mkdir(parents=True,exist_ok=True)
    if not header.exists() or header.read_text()!=text:
        header.write_text(text)


if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ['manifest','chromium','destination','header']:
        p.add_argument('--'+name,required=True,type=Path)
    args=p.parse_args()
    build(args.manifest,args.chromium,args.destination,args.header)
