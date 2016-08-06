/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * file.h
 *
 * Header file of functions that handle files and directories
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#ifndef FILE_INC_
#define FILE_INC_

// Creates a directory and sets permissions that allow creation of new files
gboolean make_directory(const char *dirname);

// Extracts the directory name from a full path name
const char *get_trunc_filename(const char *FileName);

// Returns the file length
long get_filesize(const char *FileName);

// Returns a XOR HASH value for the contents of a file
u_int fhash(FILE *f);

#endif
