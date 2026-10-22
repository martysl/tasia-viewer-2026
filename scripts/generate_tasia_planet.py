#!/usr/bin/env python3
"""Original procedural Earth-like atlas art; no Google/NASA assets or web API."""
from pathlib import Path
import numpy as np
from PIL import Image

width, height = 1536, 768
lon = np.linspace(-np.pi, np.pi, width)[None, :]
lat = np.linspace(np.pi/2, -np.pi/2, height)[:, None]
terrain = (np.sin(lon*2 + np.sin(lat*3))*.43 + np.cos(lon*3-lat*4)*.31
           + np.sin(lon*5+lat*7)*.15 + np.cos(lon*11-lat*13)*.07
           + np.sin(lon*23+lat*19)*.025)
land = terrain > .12
altitude = np.clip((terrain-.12)*1.7, 0, 1)
warm = np.cos(lat)**2 + np.zeros_like(lon)
water = np.stack(np.broadcast_arrays(9+altitude*0, 47+terrain*12, 89+terrain*28), axis=-1)
vegetation = np.stack((43+altitude*100 + (1-warm)*20,
                       105+altitude*48, 53+altitude*65), axis=-1)
rgb = np.where(land[..., None], vegetation, water)
ice = np.broadcast_to(np.abs(lat) > 1.35 - .07*np.sin(lon*7), (height, width))
rgb[ice] = (202, 223, 232)
cloud = np.clip((np.sin(lon*9+lat*15 + np.sin(lon*4)) + np.cos(lat*21-lon*6)-1.25)*.35, 0, .38)
rgb = rgb*(1-cloud[..., None]) + np.array([225,235,243])*cloud[..., None]
image = Image.fromarray(np.uint8(np.clip(rgb, 0, 255)), 'RGB')
target = Path(__file__).resolve().parents[1]/'indra/newview/skins/default/textures/world/tasia_planet.png'
target.parent.mkdir(parents=True, exist_ok=True)
image.save(target, optimize=True)
