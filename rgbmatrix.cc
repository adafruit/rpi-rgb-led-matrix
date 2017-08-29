/*------------------------------------------------------------------------
  Python module to control Adafruit RGB Matrix HAT for Raspberry Pi.

  Very very simple at the moment...just a wrapper around the SetPixel,
  Fill, Clear and SetPWMBits functions of the librgbmatrix library.
  Graphics primitives can be handled using the Python Imaging Library
  and the SetImage method.
  ------------------------------------------------------------------------*/

#include <python2.7/Python.h>
#include <python2.7/Imaging.h>
#include "led-matrix.h"
#include "graphics.h"

using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;

static GPIO io;

typedef struct { // Python object for matrix
       PyObject_HEAD
       RGBMatrix *matrix;
} RGBmatrixObject;

// Rows & chained display values are currently both required parameters.
// Might expand this to handle kwargs, etc.
static PyObject *RGBmatrix_new(
  PyTypeObject *type, PyObject *arg, PyObject *kw) {
	RGBmatrixObject *self = NULL;
	int              rows, chain;

	if((PyTuple_Size(arg) == 2) &&
	   PyArg_ParseTuple(arg, "II", &rows, &chain)) {
		if((self = (RGBmatrixObject *)type->tp_alloc(type, 0))) {
			self->matrix = new RGBMatrix(&io, rows, chain);
			Py_INCREF(self);
		}
	}

	return (PyObject *)self;
}

static int RGBmatrix_init(RGBmatrixObject *self, PyObject *arg) {
	self->matrix->Clear();
	return 0;
}

static void RGBmatrix_dealloc(RGBmatrixObject *self) {
	delete self->matrix;
	self->ob_type->tp_free((PyObject *)self);
}

static PyObject *Clear(RGBmatrixObject *self) {
	self->matrix->Clear();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Fill(RGBmatrixObject *self, PyObject *arg) {
	uint32_t c;
	uint8_t  r, g, b;
	int      status=0;

	switch(PyTuple_Size(arg)) {
	   case 1: // Packed color
		if((status = PyArg_ParseTuple(arg, "I", &c))) {
			r = (c >> 16) & 0xFF;
			g = (c >>  8) & 0xFF;
			b =  c        & 0xFF;
		}
		break;
	   case 3: // R, G, B
		status = PyArg_ParseTuple(arg, "BBB", &r, &g, &b);
		break;
	}

	if(status) self->matrix->Fill(r, g, b);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SetPixel(RGBmatrixObject *self, PyObject *arg) {
	uint32_t x, y, c;
	uint8_t  r, g, b;
	int      status=0;

	switch(PyTuple_Size(arg)) {
	   case 3: // X, Y, packed color
		if((status = PyArg_ParseTuple(arg, "IIBBB", &x, &y, &c))) {
			r = (c >> 16) & 0xFF;
			g = (c >>  8) & 0xFF;
			b =  c        & 0xFF;
		}
		break;
	   case 5: // X, Y, R, G, B
		status = PyArg_ParseTuple(arg, "IIBBB", &x, &y, &r, &g, &b);
		break;
	}

	if(status) self->matrix->SetPixel(x, y, r, g, b);

	Py_INCREF(Py_None);
	return Py_None;
}

// Copy whole display buffer to display from a list of bytes [R1,G1,B1,R2,G2,B2...]
static PyObject *SetBuffer(RGBmatrixObject *self, PyObject *data)
{
	Py_ssize_t count;
	int        w, h, offset, y, x;
	uint8_t    r, g, b;

	count = PyList_Size(data);

	w = self->matrix->width();  // Matrix dimensions
	h = self->matrix->height();
	if(count != w*h*3)
	{
		PyErr_SetString(PyExc_ValueError,
		  "Data buffer incorrect size.");
		return NULL;
	}

	for(y=0; y<h; y++)
	{
		for(x=0; x<w; x++)
		{
			offset = (y*w*3)+(x*3);
			r = PyInt_AsLong(PyList_GetItem(data, offset));
			g = PyInt_AsLong(PyList_GetItem(data, offset+1));
			b = PyInt_AsLong(PyList_GetItem(data, offset+2));
			self->matrix->SetPixel(x, y, r, g, b);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

// Copy Python Image to matrix.  Not all modes are supported;
// RGB (or RGBA, but alpha is ignored), 8-bit paletted and 1-bit.
static PyObject *SetImage(RGBmatrixObject *self, PyObject *arg) {

	Imaging  im;
	long     id;
	int      status=0, mw, mh, sx=0, sy=0, dx=0, dy=0, n, x, y, w, h;
	uint32_t c;
	uint8_t  r, g, b;

	switch(PyTuple_Size(arg)) {
	   case 1: // Image ID
		status = PyArg_ParseTuple(arg, "L", &id);
		break;
	   case 3: // Image ID, dest X & Y
		status = PyArg_ParseTuple(arg, "LII", &id, &dx, &dy);
		break;
	}

	if(status) {
		im = (Imaging)id;            // -> Imaging struct
		mw = self->matrix->width();  // Matrix dimensions
		mh = self->matrix->height();
		w  = im->xsize;
		h  = im->ysize;
		n  = dx + w - 1;             // x2
		if(n >= mw) w -= n - mw + 1; // Clip right
		n  = dy + h - 1;             // y2
		if(n >= mh) h -= n - mh + 1; // Clip bottom
		if(dx < 0) {                 // Clip left
			w  += dx;
			sx -= dx;
			dx  = 0;
		}
		if(dy < 0) {                 // Clip top
			h  += dy;
			sy -= dy;
			dy  = 0;
		}

		if((w > 0) && (h > 0)) {
			if(!strncasecmp(im->mode, "RGB", 3)) {
				// RGB image; alpha ignored if present
				for(y=0; y<h; y++) {
					for(x=0; x<w; x++) {
						c = *(uint32_t *)(
						  &IMAGING_PIXEL_RGB(
						  im, sx + x, sy + y));
						b = (c >> 16) & 0xFF;
						g = (c >>  8) & 0xFF;
						r =  c        & 0xFF;
						self->matrix->SetPixel(
						  dx + x, dy + y, r, g, b);
					}
				}
			} else if(!strcasecmp(im->mode, "1")) {
				// Bitmap image
				for(y=0; y<h; y++) {
					for(x=0; x<w; x++) {
						r = IMAGING_PIXEL_1(
						  im, sx + x, sy + y) ? 255 : 0;
						self->matrix->SetPixel(
						  dx + x, dy + y, r, r, r);
					}
				}
			} else if((!strcasecmp(im->mode, "P")) &&
				  (!strncasecmp(im->palette->mode, "RGB", 3))) {
				// Paletted image (w/RGB palette)
				for(y=0; y<h; y++) {
					for(x=0; x<w; x++) {
						c = IMAGING_PIXEL_P(
						  im, sx + x, sy + y) * 4;
						r = im->palette->palette[c];
						g = im->palette->palette[c+1];
						b = im->palette->palette[c+2];
						self->matrix->SetPixel(
						  dx + x, dy + y, r, g, b);
					}
				}
			} else {
				// Unsupported image mode
			}
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SetPWMBits(RGBmatrixObject *self, PyObject *arg) {
	uint8_t b;

	if((PyTuple_Size(arg) == 1) && PyArg_ParseTuple(arg, "B", &b)) {
		self->matrix->SetPWMBits(b);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *SetWriteCycles(RGBmatrixObject *self, PyObject *arg) {
	uint8_t b;

	if((PyTuple_Size(arg) == 1) && PyArg_ParseTuple(arg, "B", &b)) {
		io.writeCycles = b;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *DrawText(RGBmatrixObject *self, PyObject *arg) {
	const char *bdf_font_file = NULL;
	rgb_matrix::Font font;
	uint32_t x, y;
	uint8_t  r, g, b;
	char *utf8_text; // NOTE this should be UTF-8 encoded text
	if((PyTuple_Size(arg) == 7) &&
	  PyArg_ParseTuple(arg, "sIIBBBs", &bdf_font_file, &x, &y, &r, &g, &b, &utf8_text)) {
		rgb_matrix::Color color(r, g, b);
		if (!font.LoadFont(bdf_font_file)) {
			PyErr_SetString(PyExc_ValueError,
			  "Can not read BDF font file.");
			return NULL;
		}
		rgb_matrix::DrawText(
		  self->matrix, font, x, y + font.baseline(), color, utf8_text);
	}
	// TODO pass the integer returned by DrawText back to Python

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef methods[] = {
  { "Clear"         , (PyCFunction)Clear         , METH_NOARGS , NULL },
  { "Fill"          , (PyCFunction)Fill          , METH_VARARGS, NULL },
  { "SetBuffer"     , (PyCFunction)SetBuffer     , METH_O,       NULL },
  { "SetPixel"      , (PyCFunction)SetPixel      , METH_VARARGS, NULL },
  { "SetImage"      , (PyCFunction)SetImage      , METH_VARARGS, NULL },
  { "SetPWMBits"    , (PyCFunction)SetPWMBits    , METH_VARARGS, NULL },
  { "SetWriteCycles", (PyCFunction)SetWriteCycles, METH_VARARGS, NULL },
  { "DrawText"      , (PyCFunction)DrawText      , METH_VARARGS, NULL },
  { NULL, NULL, 0, NULL }
};

static PyTypeObject RGBmatrixObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                              // ob_size (not used, always set to 0)
	"rgbmatrix.Adafruit_RGBmatrix", // tp_name (module name, object name)
	sizeof(RGBmatrixObject),        // tp_basicsize
	0,                              // tp_itemsize
	(destructor)RGBmatrix_dealloc,  // tp_dealloc
	0,                              // tp_print
	0,                              // tp_getattr
	0,                              // tp_setattr
	0,                              // tp_compare
	0,                              // tp_repr
	0,                              // tp_as_number
	0,                              // tp_as_sequence
	0,                              // tp_as_mapping
	0,                              // tp_hash
	0,                              // tp_call
	0,                              // tp_str
	0,                              // tp_getattro
	0,                              // tp_setattro
	0,                              // tp_as_buffer
	Py_TPFLAGS_DEFAULT,             // tp_flags
	0,                              // tp_doc
	0,                              // tp_traverse
	0,                              // tp_clear
	0,                              // tp_richcompare
	0,                              // tp_weaklistoffset
	0,                              // tp_iter
	0,                              // tp_iternext
	methods,                        // tp_methods
	0,                              // tp_members
	0,                              // tp_getset
	0,                              // tp_base
	0,                              // tp_dict
	0,                              // tp_descr_get
	0,                              // tp_descr_set
	0,                              // tp_dictoffset
	(initproc)RGBmatrix_init,       // tp_init
	0,                              // tp_alloc
	RGBmatrix_new,                  // tp_new
	0,                              // tp_free
};

PyMODINIT_FUNC initrgbmatrix(void) { // Module initialization function
	PyObject* m;

	if(io.Init() &&    // Set up GPIO pins.  MUST run as root.
	  (m = Py_InitModule("rgbmatrix", methods)) &&
	  (PyType_Ready(&RGBmatrixObjectType) >= 0)) {
		Py_INCREF(&RGBmatrixObjectType);
		PyModule_AddObject(m, "Adafruit_RGBmatrix",
		  (PyObject *)&RGBmatrixObjectType);
	}
}
