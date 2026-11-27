import json
import re
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def png_info(path):
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG')
    width, height, depth, colour = struct.unpack('>IIBB', data[16:26])
    return width, height, colour


class ProjectTests(unittest.TestCase):
    def test_one_hundred_unique_presets(self):
        files = sorted((ROOT / 'Presets').glob('[0-9][0-9][0-9]_*.json'))
        self.assertEqual(100, len(files))
        records = [json.loads(p.read_text(encoding='utf-8')) for p in files]
        self.assertEqual(list(range(100)), [r['index'] for r in records])
        self.assertEqual(100, len({r['name'] for r in records}))
        self.assertEqual('Nebula Drift', records[0]['name'])
        self.assertEqual([72, 58, 63, 41], [records[0][x] for x in ('zoom', 'spread', 'distance', 'motion')])
        for r in records:
            for k in ('zoom', 'spread', 'distance', 'motion', 'mix', 'tone'):
                self.assertGreaterEqual(r[k], 0)
                self.assertLessEqual(r[k], 100)
            self.assertGreaterEqual(r['feedback'], 0)
            self.assertLessEqual(r['feedback'], 85)
            self.assertIn(r['mode'], ['Telescope','Orbit','Warp','Dimension','Atmosphere'])

    def test_cpp_factory_sync(self):
        header = (ROOT / 'Source/FactoryPresets.h').read_text(encoding='utf-8')
        entries = re.findall(r'^  \{"(.+?)",', header, re.M)
        self.assertEqual(100, len(entries))
        index = json.loads((ROOT / 'Presets/index.json').read_text(encoding='utf-8'))
        self.assertEqual(entries, [p['name'] for p in index])
        self.assertEqual(100, len(index))

    def test_generated_assets(self):
        expected = {
            'interface-preview.png', 'knob-hires.png', 'galaxy-hud-hires.png',
            'button-telescope-active.png', 'button-orbit-inactive.png',
            'preset-selector.png', 'waveform-meter.png', 'nebula-background.png',
            'radio-option-group.png'
        }
        assets = ROOT / 'Assets/Generated'
        self.assertEqual(expected, {p.name for p in assets.glob('*.png')})
        for name in expected:
            w, h, c = png_info(assets / name)
            self.assertGreaterEqual(w, 1000)
            self.assertGreaterEqual(h, 700)
            if name not in {'interface-preview.png', 'nebula-background.png'}:
                self.assertEqual(6, c, name + ' should contain RGBA transparency')

    def test_ci_and_runtime_files(self):
        for filename in ['gui_background.png', 'knob.png', 'galaxy.png']:
            self.assertTrue((ROOT / 'Assets/Runtime' / filename).exists())
        workflow = (ROOT / '.github/workflows/build.yml').read_text()
        self.assertIn('windows-2022', workflow)
        self.assertIn('macos-15', workflow)
        self.assertIn('DA_Telescope_AU', workflow)
        self.assertIn('DA_Telescope_VST3', workflow)


if __name__ == '__main__':
    unittest.main()
