#!/usr/bin/env python3
"""Build the Polish status bar as a full mirror of the English layout.

The Polish file used to be a reduced stub without the volume button, the
preset icons, the menu search or the QUIC indicator, so those controls were
simply absent in the Polish UI. This generates the same structure with Polish
labels, and the result is checked against the English control list.
"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
EN = ROOT/'indra/newview/skins/default/xui/en/panel_status_bar.xml'
PL = ROOT/'indra/newview/skins/default/xui/pl/panel_status_bar.xml'

STRINGS = {
    'packet_loss_tooltip': 'Utracone pakiety',
    'bandwidth_tooltip': 'Przepustowość',
    'buycurrencylabel': 'L$ [AMT]',
    'quic_disconnected': 'QUIC: nieużywany — brak połączenia z regionem.',
    'quic_secondlife': 'QUIC: nieużywany — Second Life korzysta ze standardowego transportu UDP/HTTP.',
    'quic_disabled': 'QUIC: nieużywany — wyłączony; standardowy transport UDP/HTTP.',
    'quic_unavailable': 'QUIC: nieużywany — biblioteka niedostępna; standardowy transport UDP/HTTP.',
    'quic_not_routed': 'QUIC: nieużywany — sesja symulatora niegotowa lub działa awaryjny transport UDP. Usługi HTTP pozostają bez zmian.',
    'quic_active': 'QUIC: aktywny — bieżący symulator uwierzytelnił sesję i potwierdził gotowość. Usługi HTTP pozostają bez zmian.',
}
LABELS = {
    'Search Menus': 'Szukaj w menu',
    'Type the search term you are interested in here. Results will be displayed for partial fulltext matches within the menu.':
        'Wpisz szukany termin. Wyniki pojawią się w menu.',
    'Click to refresh your L$ balance.': 'Kliknij, aby odświeżyć saldo L$.',
    'Double-click to display or hide your L$ balance.': 'Kliknij dwukrotnie, aby pokazać lub ukryć saldo L$.',
    'Click to buy more L$': 'Kliknij, aby kupić więcej L$.',
    'Open Second Life Marketplace': 'Otwórz marketplace',
    'Current time (Pacific)': 'Obecny czas (Pacyficzny)',
    'Start/Stop All Media (Music, Video, Web pages)': 'Odtwórz/Zatrzymaj wszystkie media (Muzyka, Wideo, WWW)',
}


def build():
    tree = ET.parse(EN)
    root = tree.getroot()
    for node in root.iter():
        if node.tag == 'panel.string' and node.get('name') in STRINGS:
            node.text = STRINGS[node.get('name')]
        elif node.tag in ('text', 'button', 'icon', 'search_editor', 'search_button', 'clear_button'):
            for attribute in ('label', 'tool_tip'):
                value = node.get(attribute)
                if value in LABELS:
                    node.set(attribute, LABELS[value])
    ET.indent(tree, space='  ')
    body = ET.tostring(root, encoding='unicode')
    header = '<?xml version="1.0" encoding="utf-8" standalone="yes" ?>\n'
    PL.write_text(header + body + '\n')


def controls(path):
    names = set()
    for node in ET.parse(path).getroot().iter():
        if node.get('name'):
            names.add(node.get('name'))
    return names


if __name__ == '__main__':
    build()
    english = controls(EN)
    polish = controls(PL)
    missing = sorted(english - polish)
    if missing:
        print('Polish status bar is missing:', missing)
        sys.exit(1)
    print('Polish status bar mirrors the English layout:', len(english), 'named controls')
