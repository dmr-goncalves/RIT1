/*****************************************************************************\
 * Redes Integradas de Telecomunicacoes I
 * MIEEC - FCT/UNL  2015/2016
 *
 * file.h
 *
 * Functions that handle files and directories
 *
 * Updated on September 25, 19:00
 * @author  Luis Bernardo, Rodolfo Oliveira
\*****************************************************************************/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>



// Creates a directory and sets permissions that allow creation of new files
gboolean make_directory(const char *dirname) {
  return !mkdir(dirname, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)
	 || (errno==EEXIST);
}


// Extracts the directory name from a full path name
const char *get_trunc_filename(const char *FileName) {
  char *pt= strrchr(FileName, (int)'/');
  if (pt != NULL)
    return pt+1;
  else
    return FileName;
}


// Returns the file length
long get_filesize(const char *FileName)
{
  struct stat file;
  if(!stat(FileName, &file))
  {
    return file.st_size;
  }
  return 0;
}


// Returns a XOR HASH value for the contents of a file
u_int fhash(FILE *f) {
  assert(f != NULL);
  rewind(f);
  u_int sum= 0;
  int aux= 0;
  while (fread(&aux, 1, sizeof(int), f) > 0)
      sum ^= aux;
  return sum;
}

