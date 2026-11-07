#pragma once
// DisplayDriver names pax_buf_t in its menu overlay hook. The host build never
// draws a menu, so an opaque type is enough to compile against.
typedef struct pax_buf pax_buf_t;
