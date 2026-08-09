/**
 * Icon geometry, extracted from Lucide (ISC License).
 *
 * Stored as structured shapes rather than markup strings so the renderer
 * never needs {@html}, which is banned. Extraction allowlists both the SVG
 * element names and their attributes, so nothing but geometry survives.
 *
 * Inlined rather than fetched, so pages need no external host and satisfy
 * img-src 'self'. Only the icons in use are here and the package was removed
 * after extraction, so nothing ships at runtime.
 *
 * Upstream: https://lucide.dev  Licence: static/icons/LUCIDE-LICENSE.txt
 */

export interface IconShape {
	tag: string;
	attrs: Record<string, string>;
}

export const ICONS: Record<string, IconShape[]> = {
 "search": [
  {
   "tag": "path",
   "attrs": {
    "d": "m21 21-4.34-4.34"
   }
  },
  {
   "tag": "circle",
   "attrs": {
    "cx": "11",
    "cy": "11",
    "r": "8"
   }
  }
 ],
 "bell": [
  {
   "tag": "path",
   "attrs": {
    "d": "M10.268 21a2 2 0 0 0 3.464 0"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M3.262 15.326A1 1 0 0 0 4 17h16a1 1 0 0 0 .74-1.673C19.41 13.956 18 12.499 18 8A6 6 0 0 0 6 8c0 4.499-1.411 5.956-2.738 7.326"
   }
  }
 ],
 "trophy": [
  {
   "tag": "path",
   "attrs": {
    "d": "M10 14.66V17a1 1 0 0 1-1 1 2 2 0 0 0-2 2v2"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M14 14.66V17a1 1 0 0 0 1 1 2 2 0 0 1 2 2v2"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M17.916 10H19.5A2.5 2.5 0 0 0 22 7.5V5a1 1 0 0 0-1-1h-3"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M4 22h16"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M6 9a6 6 0 0 0 12 0V3a1 1 0 0 0-1-1H7a1 1 0 0 0-1 1z"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M6.084 10H4.5A2.5 2.5 0 0 1 2 7.5V5a1 1 0 0 1 1-1h3"
   }
  }
 ],
 "help": [
  {
   "tag": "circle",
   "attrs": {
    "cx": "12",
    "cy": "12",
    "r": "10"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M12 17h.01"
   }
  }
 ],
 "home": [
  {
   "tag": "path",
   "attrs": {
    "d": "M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M3 10a2 2 0 0 1 .709-1.528l7-6a2 2 0 0 1 2.582 0l7 6A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
   }
  }
 ],
 "questions": [
  {
   "tag": "path",
   "attrs": {
    "d": "M22 17a2 2 0 0 1-2 2H6.828a2 2 0 0 0-1.414.586l-2.202 2.202A.71.71 0 0 1 2 21.286V5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2z"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M7 11h10"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M7 15h6"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M7 7h8"
   }
  }
 ],
 "experiences": [
  {
   "tag": "path",
   "attrs": {
    "d": "M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M14 2v5a1 1 0 0 0 1 1h5"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M10 9H8"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M16 13H8"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M16 17H8"
   }
  }
 ],
 "tag": [
  {
   "tag": "path",
   "attrs": {
    "d": "M12.586 2.586A2 2 0 0 0 11.172 2H4a2 2 0 0 0-2 2v7.172a2 2 0 0 0 .586 1.414l8.704 8.704a2.426 2.426 0 0 0 3.42 0l6.58-6.58a2.426 2.426 0 0 0 0-3.42z"
   }
  },
  {
   "tag": "circle",
   "attrs": {
    "cx": "7.5",
    "cy": "7.5",
    "r": ".5"
   }
  }
 ],
 "saved": [
  {
   "tag": "path",
   "attrs": {
    "d": "M17 3a2 2 0 0 1 2 2v15a1 1 0 0 1-1.496.868l-4.512-2.578a2 2 0 0 0-1.984 0l-4.512 2.578A1 1 0 0 1 5 20V5a2 2 0 0 1 2-2z"
   }
  }
 ],
 "submit": [
  {
   "tag": "rect",
   "attrs": {
    "width": "18",
    "height": "18",
    "x": "3",
    "y": "3",
    "rx": "2"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M8 12h8"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M12 8v8"
   }
  }
 ],
 "activity": [
  {
   "tag": "path",
   "attrs": {
    "d": "M22 12h-2.48a2 2 0 0 0-1.93 1.46l-2.35 8.36a.25.25 0 0 1-.48 0L9.24 2.18a.25.25 0 0 0-.48 0l-2.35 8.36A2 2 0 0 1 4.49 12H2"
   }
  }
 ],
 "companies": [
  {
   "tag": "path",
   "attrs": {
    "d": "M10 12h4"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M10 8h4"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M14 21v-3a2 2 0 0 0-4 0v3"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M6 10H4a2 2 0 0 0-2 2v7a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-2"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M6 21V5a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v16"
   }
  }
 ],
 "moderation": [
  {
   "tag": "path",
   "attrs": {
    "d": "M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "m9 12 2 2 4-4"
   }
  }
 ],
 "reports": [
  {
   "tag": "path",
   "attrs": {
    "d": "M4 22V4a1 1 0 0 1 .4-.8A6 6 0 0 1 8 2c3 0 5 2 7.333 2q2 0 3.067-.8A1 1 0 0 1 20 4v10a1 1 0 0 1-.4.8A6 6 0 0 1 16 16c-3 0-5-2-8-2a6 6 0 0 0-4 1.528"
   }
  }
 ],
 "audit": [
  {
   "tag": "path",
   "attrs": {
    "d": "M15 12h-5"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M15 8h-5"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M19 17V5a2 2 0 0 0-2-2H4"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M8 21h12a2 2 0 0 0 2-2v-1a1 1 0 0 0-1-1H11a1 1 0 0 0-1 1v1a2 2 0 1 1-4 0V5a2 2 0 1 0-4 0v2a1 1 0 0 0 1 1h3"
   }
  }
 ],
 "close": [
  {
   "tag": "path",
   "attrs": {
    "d": "M18 6 6 18"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "m6 6 12 12"
   }
  }
 ],
 "menu": [
  {
   "tag": "path",
   "attrs": {
    "d": "M4 5h16"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M4 12h16"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M4 19h16"
   }
  }
 ],
 "user": [
  {
   "tag": "circle",
   "attrs": {
    "cx": "12",
    "cy": "8",
    "r": "5"
   }
  },
  {
   "tag": "path",
   "attrs": {
    "d": "M20 21a8 8 0 0 0-16 0"
   }
  }
 ],
 "chevron": [
  {
   "tag": "path",
   "attrs": {
    "d": "m9 18 6-6-6-6"
   }
  }
 ]
};

export type IconName = keyof typeof ICONS;
