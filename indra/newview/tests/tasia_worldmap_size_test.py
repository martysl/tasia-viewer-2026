#!/usr/bin/env python3
"""MapBlockReply optional-size integration and legacy-default contracts."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
NV = ROOT / 'indra/newview'


class WorldMapSize(unittest.TestCase):
    def test_message_template_declares_optional_size_pair(self):
        template = (ROOT / 'scripts/messages/message_template.msg').read_text()
        block = template.split('MapBlockReply Low 409', 1)[1].split('MapItemRequest Low 410', 1)[0]
        self.assertIn('Size            Variable', block)
        self.assertIn('{   SizeX       U16', block)
        self.assertIn('{   SizeY       U16', block)

    def test_reply_parser_is_opensim_scoped_and_bounded(self):
        source = (NV / 'llworldmapmessage.cpp').read_text()
        body = source.split('void LLWorldMapMessage::processMapBlockReply(', 1)[1]
        self.assertIn('LLGridManager::getInstance()->isInOpenSim()', body)
        self.assertIn('getNumberOfBlocksFast(_PREHASH_Size)', body)
        self.assertIn('size_blocks == num_blocks', body)
        self.assertIn('validMapDimensions(size_x, size_y)', body)
        self.assertIn('LLTasiaRegion::DEFAULT_WIDTH', body)
        self.assertIn('insertRegion(x_world, y_world, size_x, size_y', body)

    def test_second_life_and_legacy_replies_keep_256m_defaults(self):
        parser = (NV / 'llworldmapmessage.cpp').read_text()
        model = (NV / 'llworldmap.cpp').read_text()
        self.assertIn('U16 size_x = LLTasiaRegion::DEFAULT_WIDTH;', parser)
        self.assertIn('U16 size_y = LLTasiaRegion::DEFAULT_WIDTH;', parser)
        self.assertIn('return insertRegion(x_world, y_world, LLTasiaRegion::DEFAULT_WIDTH,', model)
        self.assertIn('mSizeX(LLTasiaRegion::DEFAULT_WIDTH)', model)
        self.assertIn('mSizeY(LLTasiaRegion::DEFAULT_WIDTH)', model)

    def test_world_map_stores_and_draws_region_extents(self):
        header = (NV / 'llworldmap.h').read_text()
        model = (NV / 'llworldmap.cpp').read_text()
        view = (NV / 'llworldmapview.cpp').read_text()
        self.assertIn('U16 getSizeX() const { return mSizeX; }', header)
        self.assertIn('U16 getSizeY() const { return mSizeY; }', header)
        self.assertIn('siminfo->setSize(size_x, size_y)', model)
        self.assertIn('validMapDimensions(size_x, size_y)', model)
        self.assertIn('x_world + size_x, y_world + size_y', model)
        self.assertIn('info->getSizeX() / REGION_WIDTH_METERS', view)
        self.assertIn('info->getSizeY() / REGION_WIDTH_METERS', view)


if __name__ == '__main__':
    unittest.main(verbosity=2)
