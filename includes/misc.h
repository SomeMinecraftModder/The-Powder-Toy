/**
 * Powder Toy - miscellaneous functions (header)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <string>

//Signum function
int isign(float i);

unsigned clamp_flt(float f, float min, float max);

float restrict_flt(float f, float min, float max);

char *mystrdup(const char *s);

struct strlist
{
	char *str;
	struct strlist *next;
};

void strlist_add(struct strlist **list, char *str);

int strlist_find(struct strlist **list, char *str);

void strlist_free(struct strlist **list);

void save_presets();

void load_presets(void);

void clean_text(char *text, int vwidth);

int sregexp(const char *str, const char *pattern);

void strcaturl(char *dst, char *src);

void strappend(char *dst, const char *src);

std::string CleanString(std::string dirtyString, bool ascii, bool color, bool newlines, bool numeric = false);
std::string CleanString(const char * dirtyData, bool ascii, bool color, bool newlines, bool numeric = false);

int file_exists(const char *filename);
void *file_load(const char *fn, int *size);

void clipboard_push_text(char * text);

char * clipboard_pull_text();

extern char *clipboard_text;

void HSV_to_RGB(int h,int s,int v,int *r,int *g,int *b);

void RGB_to_HSV(int r,int g,int b,int *h,int *s,int *v);

#ifdef __cplusplus
class Tool;
Tool* GetToolFromIdentifier(std::string identifier, std::string def="");

std::string URLEncode(std::string source);
#endif

void membwand(void * dest, void * src, size_t destsize, size_t srcsize);

extern bool doingUpdate;

#endif
