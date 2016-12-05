#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include <stdio.h>

#include "utils.h"
#include "wmail.h"

static char hex2int(int c)
{
	if (isdigit(c)) {
		return c - '0';
	}
	else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	else {
		return -1;
	}
}

/*
*
* Decoding  Quoted-printable string.
*
*/
/* {{{ proto string quoted_printable_decode(string str)
   Convert a quoted-printable string to an 8 bit string */
char *quoted_printable_decode(char *str_in,char *str_out,int header) {
	int i = 0, j = 0, k;

	str_out = malloc(strlen(str_in) + 1);
	while (str_in[i]) {
		switch (str_in[i]) {
		case '=':
			if (str_in[i + 1] && str_in[i + 2] && 
				isxdigit((int) str_in[i + 1]) && 
				isxdigit((int) str_in[i + 2]))
			{
				str_out[j++] = (hex2int((int) str_in[i + 1]) << 4) 
						+ hex2int((int) str_in[i + 2]);
				i += 3;
			} else  /* check for soft line break according to RFC 2045*/ {
				k = 1;
				while (str_in[i + k] && ((str_in[i + k] == 32) || (str_in[i + k] == 9))) {
					/* Possibly, skip spaces/tabs at the end of line */
					k++;
				}
				if (!str_in[i + k]) {
					/* End of line reached */
					i += k;
				}
				else if ((str_in[i + k] == 13) && (str_in[i + k + 1] == 10)) {
					/* CRLF */
					i += k + 2;
				}
				else if ((str_in[i + k] == 13) || (str_in[i + k] == 10)) {
					/* CR or LF */
					i += k + 1;
				}
				else {
					str_out[j++] = str_in[i++];
				}
			}
			break;
		case '_':
			if (header) {
				str_out[j++] = 0x20;
				++i;
			} else {
				str_out[j++] = str_in[i++];
			}
			break;
		default:
			str_out[j++] = str_in[i++];
		}
	}
	str_out[j] = '\0';
 
	return str_out;   
}

char *quoted_printable_encode(char *str, int str_len, int binary_mode) {

	long line_len = 76;
	char *str_in, *str_out;
	int i = 0, j = 0, l = 0;
	int buf_len;


	str_in = str;
	buf_len = (str_len * 3) + (((str_len * 3 / line_len) + 1) * 3) + 1;
	str_out = (char *) malloc(buf_len);
	if (!str_out) return NULL;

	while (i < str_len) {
		if (j >= buf_len) {
			SETWARN(E_PARSE,"QENCODE: invalid memory pointer");
		}
		if ((binary_mode && ((str_in[i] == 0x20) || (str_in[i] == 0x09) ||
				(str_in[i] == 0x0A) || (str_in[i] == 0x0D))) ||
				(((str_in[i] < 0x20) && (str_in[i] != 0x09) &&
				(str_in[i] != 0x0A) && (str_in[i] != 0x0D)) ||
				(str_in[i] == 0x3D) || (str_in[i] > 0x7E))) {
			if ((line_len > 0) && (l + 3 > line_len - 1)) {
				str_out[j++] = '=';
				str_out[j++] = 0x0D;
				str_out[j++] = 0x0A;
				l = 0;
			}
			j += sprintf(str_out + j, "=%02X", (unsigned char)(str_in[i++]));
			l += 3;
		} else {
			if ((line_len > 0) && (l + 1 > line_len - 1)
					&& (str_in[i+1] != 0x0A) && (str_in[i+1] != 0x0D)) {
				str_out[j++] = '=';
				str_out[j++] = 0x0D;
				str_out[j++] = 0x0A;
				l = 0;
			}
			if (!binary_mode && ((str_in[i] == 0x0A) || (str_in[i] == 0x0D))) {
				l = 0;
			} else {
				l++;
			}
			str_out[j++] = str_in[i++];
		}
	}
	str_out[j] = '\0';

	return str_out;
}

