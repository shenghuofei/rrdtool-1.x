/****************************************************************************
 * RRDtool 1.2.7  Copyright by Tobi Oetiker, 1997-2005
 ****************************************************************************
 * rrd_afm_data.h  Encoded afm (Adobe Font Metrics) for selected fonts.
 ****************************************************************************/

#ifndef  RRD_AFM_DATA_H
#define RRD_AFM_DATA_H

/*
Description of data structures:

  Ideally, the struct should be a list of fonts, and each font
  is a list of character-info.
  Each character has a structure:
    struct charinfo {
      char16 thechar;
      int width;
      struct {
	char16 nextchar;
	int deltawidth;
      } kernings[];
      struct {
	char16 nextchar;
	char16 resultingchar;
      } ligatures[];
    }

    The data for typical fonts makes this a very sparse data structure.
    For most fonts, only the letter "f" has ligatures.
    All fonts have all (or almost all) of the characters 32-126,
    most fonts have all 161-255,
    and all fonts have very few 256-65535.
    Most kerning pairs have both chars 32-126.

    The most basic design decision�is to have all this data as 
    const C globals all set up by array/struct initialisers
    so runtime setup overhead is minimal.
    The complete other possibility would be to parse and load
    this info at runtime, but for rrdtool I have preferred
    speed for flexibility as the same few fonts will be used 
    zillions of times.

    So the idea is to rewrite the above structure into
    something which:
    1) uses/wastes minimal memory
    2) is fast for most characters
    3) supports at least Iso-Latin-1, prefer full unicode.
    4) doesn't need full precision in char width
    	(we can afford to loose 0.2% as rrdtool only needs to calculate
	overall layout of elements, not positioning individual
	characters)
    5) can be written as constant initialisers to C structs/arrays
       so we don't have runtime overhead starting rrdtool.
    6) can be easily generated by some script so it is easy
       to select a set of fonts and have the C data updated.
       So adding/removing fonts is a matter of a recompile.

Implementation design:
    All character structs are sorted by unicode value. Info for
    characters below 32 is discarded and the chars are treated 
    as a space. Missing characters in the 32-126 range are 
    substituted with default values so we can use direct array 
    access for those. For characters above 126, binary search 
    is used (not yet, liniar now but uses good guess for most latin 1
    characters).

    Ligature handling can be discarded as ligatures have very small
    effects on string width. The width of the "fi" ligature
    is the same (or very close) to the width of "f" plus the width 
    of "i".
    If implemented, it can be a simple list (global for the font,
    not for each character) because all fonts I've seen that have
    ligatures has max 3 pairs: "fi", "fl", "ffl" and no other. 

    Most characters has less than 10 kern pairs, few 10-20, and
    extremly few 20-30. This is implemented as a simple
    linear search with characters 256-65536 encoding using a prefix
    so most kern pairs only take 2 bytes:
    unsigned 8 bit char value and signed 8 bit kern width.
    Using a non-packed format would enable binary search, but
    would use almost twice as much memory for a yet unknown
    gain in speed.

    Character widths are stored as unsigned bytes. Width of
    one character is font-size * bytevalue * (1000 / 6)
    AFM specifies widths as integers with 1000 representing 1 * font-size.
    Kerning delta widths has same scaling factor, but the value
    is a signed byte as many kerning widths are negative and smaller
    than avarage character width.

    Kerning info is stored in a shared packed int8 array
    to reduce the number of structs and memory usage.
    This sets the maximum number of kerning pairs to
    approx 15000.
      The font I have seen with most kern pairs is
      "Bodoni Old Face BE Bold Italic Oldstyle Figures"
      which has 1718 pairs for 62 chars.
      Typical fonts have 100-150 pairs.
    For each character needs then only a 16 bit index
    into this shared table.
    The format of the sub-arrays are:
      count ( unicode deltawidth )
    with the (...) repeated count times.
    The count and the unicode is packed because a lot
    entries is less than 256, and most below 400.
    Therefore an escape sequence is used.
    If the value is >= 510
      1, high-8bits, low-8bits
    else if the value is >= 254
      0, value minus 254
    else
      value plus 1
    An index of zero is treated as a NULL pointer,
    and the first byte in a shared array is
    therefore not used (and filled with a dummy value).
    The array is only created if non-empty.
	No entries can be zero (they are redundant),
	and no subarray can be empty (as the index pointer
	then is 0 meaning no sub array).
	The deltawidth is stored as a non-escaped signed byte.

    So for each character needed info is:
      width: unsigned 8 bit int.
      kerning-subarray-index: unsigned 16 bit int.

    The first 126-32+1 entries are for the characters
    32-126. If any is missing, a dummy entry is created.
    For characters 126-65535 a font-global
    array of struct {unicode, char-index} is
    used for binary search (not yet, liniar now).

    Ligatures can be implemented as a font-global
    array of struct {
      unicode char1, char2, resultingchar;
    }

    Font-global info is stored in a struct afm_fontinfo (see below).

    The highchars_index and ligatures structures are flattened
    to a simple array to avoid accidental padding between
    structs if the structsize is problematic for some platforms.

    All fonts are stored in an array of this struct,
    sorted by fullname for binary search (not yet sorted).

    The .afm files are compiled by a perl script which creates
    rrd_afm_data.c 
    The only thing rrd_afm_data.c contains is this compiled data.

    Compiled on Mac OS X the size of rrd_afm_data.o
    is 67 Kb for the standard 14 postscript fonts,
    and 490 Kb for a set of 276 Adobe fonts.
*/

typedef unsigned char  afm_uint8;
typedef signed   char  afm_sint8;
typedef unsigned short afm_uint16;
typedef signed   short afm_sint16;
typedef unsigned short afm_unicode;

typedef const afm_uint8   afm_cuint8;
typedef const afm_sint8   afm_csint8;
typedef const afm_uint16  afm_cuint16;
typedef const afm_sint16  afm_csint16;
typedef const afm_unicode afm_cunicode;

typedef struct afm_fontinfo {
  const char   *fullname; /* e.g. "Futura Bold Oblique" */
  const char   *postscript_name; /* e.g. "Futura-BoldOblique" */
  afm_cuint16 ascender, descender;
  afm_cuint8   *widths;
  afm_csint16  *kerning_index;
  afm_cuint8   *kerning_data;
  afm_cuint16  *highchars_index;
  afm_cuint16   highchars_count;
  afm_cunicode *ligatures;
  afm_cuint16   ligatures_count;
}      afm_fontinfo;

typedef struct old_afm_fontinfo {
  const char *fontname, *fullname;
  const unsigned short *charinfo, *intarray;
  const unsigned short charinfocount;
  const unsigned short fixedpitch;
} old_afm_fontinfo;

extern const afm_fontinfo afm_fontinfolist[];
extern const int afm_fontinfo_count;

#endif
