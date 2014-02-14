/*	$OpenBSD: pkcs5_pbkdf2_test.c,v 1.1 2012/09/06 20:49:59 matthew Exp $	*/
/*-
 * Copyright (c) 2008 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <util.h>

struct test_vector {
	u_int rounds;
	const char *pass;
	const char *salt;
	const char expected[32];
};

/*
 * Test vectors from RFC 3962
 */
struct test_vector test_vectors[] = {
	{
		1,
		"password",
		"ATHENA.MIT.EDUraeburn",
		{
			0xcd, 0xed, 0xb5, 0x28, 0x1b, 0xb2, 0xf8, 0x01,
			0x56, 0x5a, 0x11, 0x22, 0xb2, 0x56, 0x35, 0x15,
			0x0a, 0xd1, 0xf7, 0xa0, 0x4b, 0xb9, 0xf3, 0xa3,
			0x33, 0xec, 0xc0, 0xe2, 0xe1, 0xf7, 0x08, 0x37
		},
	}, {
		2,
		"password",
		"ATHENA.MIT.EDUraeburn",
		{
			0x01, 0xdb, 0xee, 0x7f, 0x4a, 0x9e, 0x24, 0x3e, 
			0x98, 0x8b, 0x62, 0xc7, 0x3c, 0xda, 0x93, 0x5d,
			0xa0, 0x53, 0x78, 0xb9, 0x32, 0x44, 0xec, 0x8f,
			0x48, 0xa9, 0x9e, 0x61, 0xad, 0x79, 0x9d, 0x86
		},
	}, {
		1200,
		"password",
		"ATHENA.MIT.EDUraeburn",
		{
			0x5c, 0x08, 0xeb, 0x61, 0xfd, 0xf7, 0x1e, 0x4e,
			0x4e, 0xc3, 0xcf, 0x6b, 0xa1, 0xf5, 0x51, 0x2b,
			0xa7, 0xe5, 0x2d, 0xdb, 0xc5, 0xe5, 0x14, 0x2f,
			0x70, 0x8a, 0x31, 0xe2, 0xe6, 0x2b, 0x1e, 0x13
		},
	}, {
		5,
		"password",
		"\0224VxxV4\022", /* 0x1234567878563412 */
		{
			0xd1, 0xda, 0xa7, 0x86, 0x15, 0xf2, 0x87, 0xe6,
			0xa1, 0xc8, 0xb1, 0x20, 0xd7, 0x06, 0x2a, 0x49,
			0x3f, 0x98, 0xd2, 0x03, 0xe6, 0xbe, 0x49, 0xa6,
			0xad, 0xf4, 0xfa, 0x57, 0x4b, 0x6e, 0x64, 0xee
		},
	}, {
		1200,
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
		"pass phrase equals block size",
		{
			0x13, 0x9c, 0x30, 0xc0, 0x96, 0x6b, 0xc3, 0x2b,
			0xa5, 0x5f, 0xdb, 0xf2, 0x12, 0x53, 0x0a, 0xc9,
			0xc5, 0xec, 0x59, 0xf1, 0xa4, 0x52, 0xf5, 0xcc,
			0x9a, 0xd9, 0x40, 0xfe, 0xa0, 0x59, 0x8e, 0xd1
		},
	}, {
		1200,
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
		"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
		"pass phrase exceeds block size",
		{
			0x9c, 0xca, 0xd6, 0xd4, 0x68, 0x77, 0x0c, 0xd5,
			0x1b, 0x10, 0xe6, 0xa6, 0x87, 0x21, 0xbe, 0x61,
			0x1a, 0x8b, 0x4d, 0x28, 0x26, 0x01, 0xdb, 0x3b,
			0x36, 0xbe, 0x92, 0x46, 0x91, 0x5e, 0xc8, 0x2a
		},
	}, {
		50,
		"\360\235\204\236", /* g-clef (0xf09d849e) */
		"EXAMPLE.COMpianist",
		{
			0x6b, 0x9c, 0xf2, 0x6d, 0x45, 0x45, 0x5a, 0x43,
			0xa5, 0xb8, 0xbb, 0x27, 0x6a, 0x40, 0x3b, 0x39,
			0xe7, 0xfe, 0x37, 0xa0, 0xc4, 0x1e, 0x02, 0xc2,
			0x81, 0xff, 0x30, 0x69, 0xe1, 0xe9, 0x4f, 0x52
		},
	}
};
#define NVECS (sizeof(test_vectors) / sizeof(*test_vectors))

static void
printhex(const char *s, const u_int8_t *buf, size_t len)
{
	size_t i;

	printf("%s: ", s);
	for (i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
	fflush(stdout);
}

int
main(int argc, char **argv)
{
	u_int i, j;
	u_char result[32];
	struct test_vector *vec;

	for (i = 0; i < NVECS; i++) {
		vec = &test_vectors[i];
		for (j = 2; j <= sizeof(result); j += 3) {
			if (pkcs5_pbkdf2(vec->pass, strlen(vec->pass),
			    vec->salt, strlen(vec->salt),
			    result, j, vec->rounds) != 0)
				errx(1, "pbkdf2 vector %u failed", i);
			if (memcmp(result, vec->expected, j) != 0) {
				printhex(" got", result, j);
				printhex("want", vec->expected, j);
				return 1;
			}
		}
	}
	return 0;
}
