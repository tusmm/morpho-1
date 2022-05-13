/** @file text.c
 *  @author T J Atherton
 *
 *  @brief Text rendering using freetype
 */

#include "text.h"

FT_Library ftlibrary;

/* -------------------------------------------------------
 * UTF8 handling code
 * ------------------------------------------------------- */

/** @brief Returns the number of bytes in the next character of a given utf8 string
    @returns number of bytes */
int text_utf8numberofbytes(uint8_t *string) {
    uint8_t byte = * string;
    
    if ((byte & 0xc0) == 0x80) return 0; // In the middle of a utf8 string
    
    // Get the number of bytes from the first character
    if ((byte & 0xf8) == 0xf0) return 4;
    if ((byte & 0xf0) == 0xe0) return 3;
    if ((byte & 0xe0) == 0xc0) return 2;
    return 1;
}

/** Decodes a utf8 character.
 * @param[in] string - string to decode
 * @param[out] out - decoded character
 * @returns true on success, false otherwise */
bool text_utf8decode(const uint8_t* string, int *out) {
    if (*string <= 0x7f) { // ASCII single byte value
        *out = *string;
        return true;
    }

    int value;
    uint32_t nbytes;
    if ((*string & 0xe0) == 0xc0) { // Two byte sequence: 110xxxxx 10xxxxxx.
        value = *string & 0x1f;
        nbytes = 1;
    } else if ((*string & 0xf0) == 0xe0) { // Three byte sequence: 1110xxxx     10xxxxxx 10xxxxxx.
        value = *string & 0x0f;
        nbytes = 2;
    } else if ((*string & 0xf8) == 0xf0) { // Four byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx.
        value = *string & 0x07;
        nbytes = 3;
    } else return false; // UTF8 sequence was invalid

    for (const uint8_t *s=string+1; nbytes > 0; nbytes--, s++) {
        if ((*s & 0xc0) != 0x80) return false; // Invalid UTF8 sequence
        value = value << 6 | (*s & 0x3f);
    }
    
    *out = value;

    return true;
}

/* -------------------------------------------------------
 * Testing code
 * ------------------------------------------------------- */

/** Prints a bitmap (for testing purposes only) */
void text_drawbitmap(FT_Bitmap *bitmap) {
  FT_Int  i, j;
  FT_Int  x_max = bitmap->width;
  FT_Int  y_max = bitmap->rows;

  for (i=0; i<y_max; i++) {
    for (j=0; j<x_max; j++) {
        char c = bitmap->buffer[i * bitmap->width + j];
        printf("%c", (c==0 ? ' ' : '*'));
    }
    printf("\n");
  }
}

/* -------------------------------------------------------
 * Skyline algorithm to allocate glyphs
 * ------------------------------------------------------- */

/* The skyline algorithm facilitates allocation of rectangles within a rectangular strip.
   At any time, we store the "skyline" of the uppermost edges of existing rectangles.
   New rectangles are inserted so that they occupy the lowest possible position.

    ********************************
    *                              *
    *                          ----*
    *----*                     *   *
    *    *----------*          *   *
    *    *          *----------*   *
    *    *          *          *   *
    ********************************
 
 */

DEFINE_VARRAY(textskylineentry, textskylineentry);

/* Initializes a texture skyline
 * @param[in] skyline - skyline to initialize */
void text_skylineinit(textskyline *skyline, int width, int height) {
    skyline->width=width;
    skyline->height=height;
    varray_textskylineentryinit(&skyline->skyline);
    
    /* Initially empty skyline */
    textskylineentry def = { .xpos = 0, .ypos = 0, .width = width, .next = NULL};
    varray_textskylineentrywrite(&skyline->skyline, def);
}

/* Clears a texture skyline
 * @param[in] skyline - skyline to clear */
void text_skylineclear(textskyline *skyline) {
    varray_textskylineentryclear(&skyline->skyline);
}

/** Check if a skyline entry can fit a rectangle, looking  */
bool text_skylinetestfit(textskylineentry *start, int width, int ypos) {
    int w = width;
    for (textskylineentry *e=start; e!=NULL; e=e->next) {
        if (e->ypos>ypos) break; // Stop if the next rectangle is taller
        if (w<=e->width) return true; // We can fit the remaining width
        w-=e->width;
    }
    return false;
}

/** Fit a new rectangle into the skyline  */
bool text_skylinefit(textskyline *skyline, textskylineentry *start, int width, int height, int ypos) {
    textskylineentry *end=start; // Final element
    int w = width;
    
    for (; end!=NULL; end=end->next) { // Identify the final element in the skyline
        if (w<=end->width) break;
        w-=end->width;
    }
    
    if (start==end) { // Split the block
        if (start->width-width>0) {
            textskylineentry new = { .xpos = start->xpos+width, .ypos = start->ypos, .width = start->width-width, .next=start->next };
            if (!varray_textskylineentrywrite(&skyline->skyline, new)) return false;
            end = skyline->skyline.data+skyline->skyline.count-1;
        } else end=NULL;
    } else { // Update the end block
        end->width-=w;
        end->xpos+=w;
    }
    
    start->width=width;
    start->ypos=ypos+height;
    start->next=end;
    
    return true;
}

/* Insert a rectangle into the texture skyline
 * @param[in] skyline - skyline to use
 * @param[in] width - width of rectangle to insert
 * @param[in] height - height of rectangle to insert
 * @param[out] x - bottom left x
 * @param[out] y - bottom left y
 * @returns true if the rectangle has been successfully inserted, false if there's no more room */
bool text_skylinesinsert(textskyline *skyline, int width, int height, int *x, int *y) {
    textskylineentry *best=skyline->skyline.data;
    
    /* Locate the lowest point that can accomodate the rectangle */
    for (textskylineentry *e = skyline->skyline.data; e!=NULL; e=e->next) {
        if (text_skylinetestfit(e, width, e->ypos) && e->ypos<best->ypos) {
            best = e;
        }
    }
    
    if (text_skylinetestfit(best, width, best->ypos) && skyline->height>best->ypos+height) {
        *x=best->xpos;
        *y=best->ypos;
        return text_skylinefit(skyline, best, width, height, best->ypos);
    }
    
    return false;
}

/* -------------------------------------------------------
 * Manage fonts
 * ------------------------------------------------------- */

/* Opens a font
 * @param[in] file - Font file
 * @param[in] size - Font size
 * @param[out] font - Font record filled out
 * @returns true on success */
bool text_openfont(char *file, int size, textfont *font) {
    FT_Error error = FT_New_Face(ftlibrary, file, 0, &font->face);
    if (error) return false;
    
    error = FT_Set_Pixel_Sizes(font->face, 0, size);
    if (error) return false;
    
    return true;
}

/* Clears a font
 * @param[in] font - Font record filled out */
void text_clearfont(textfont *font) {
    FT_Done_Face(font->face);
}

/* Adds a character with code to a font
 * @param[in] font - Font record filled out
 * @param[in] code - code point to add */
bool text_addcharacter(textfont *font, int code) {
    FT_Error error;
    error = FT_Load_Char(font->face, code, FT_LOAD_RENDER);
    if (error) return false;
    text_drawbitmap(&font->face->glyph->bitmap);
    return true;
}

/* Prepares a font to display a particular piece of text
 * @param[in] font - Font record filled out
 * @param[in] text - */
bool text_prepare(textfont *font, char *text) {
    for (uint8_t *c = (uint8_t *) text; *c!='\0'; ) {
        int code;
        if (!text_utf8decode(c, &code)) return false;
        if (!text_addcharacter(font, code)) return false;
        
        c+=text_utf8numberofbytes(c);
    }

    return true;
}

/* Initialize the text library */
void text_initialize(void) {
    FT_Init_FreeType(&ftlibrary);
    
    textfont font;
    text_openfont("/Library/Fonts/Arial Unicode.ttf", 32, &font);
    //text_openfont("/System/Library/Fonts/Helvetica.ttc", 32, &font);
    
    text_prepare(&font, "Olá mundo!");
    
    /*textskyline skyline;
    
    int x, y;
    text_skylineinit(&skyline, 80, 200);
    for (int i=0; i<10; i++) {
        text_skylinesinsert(&skyline, 20, 20, &x, &y);
        printf("%i %i\n",x,y);
    }
    
    text_skylineclear(&skyline);*/
    
    text_clearfont(&font);
}

void text_finalize(void) {
    FT_Done_FreeType(ftlibrary);
}