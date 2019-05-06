#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static void usage(int argc, char* argv[]) {
	const char *prog = argc > 0 ? argv[0] : "xor";
	printf(
		"Usage: %s [OPTIONS...] KEY FILE...\n"
		"\n"
		"OPTIONS:\n"
		"\n"
		"\t-h, --help ............ this help message\n"
		"\t-x, --hex ............. key as hexadecimal values (default)\n"
		"\t-s, --str ............. key as normal string\n"
		"\t-f, --file ............ read key form file\n"
		"\t-o, --output=OUTPUT ... output file (default: stdout)\n",
		prog
	);
}

static int parse_half_hex_byte(char ch) {
	if (ch >= '0' && ch <= '9') {
		return ch - '0';
	} else if (ch >= 'a' && ch <= 'f') {
		return ch - 'a' + 10;
	} else if (ch >= 'A' && ch <= 'F') {
		return ch - 'A' + 10;
	} else {
		errno = EINVAL;
		return -1;
	}
}

static int parse_hex_byte(const char *str) {
	int hi = parse_half_hex_byte(str[0]);
	if (hi < 0) {
		return -1;
	}
	int lo = parse_half_hex_byte(str[1]);
	if (lo < 0) {
		return -1;
	}
	return (hi << 4) | lo;
}

static int fxor(FILE* infp, const char *infilename, FILE* outfp, const char *outfilename, uint8_t *key, uint8_t *buf, size_t size) {
	for (;;) {
		size_t count = fread(buf, 1, size, infp);
		if (count < size) {
			if (ferror(infp)) {
				perror(infilename);
				return -1;
			} else if (count == 0) {
				return 0;
			}
		}

		for (size_t index = 0; index < count; ++ index) {
			buf[index] ^= key[index];
		}

		if (fwrite(buf, count, 1, outfp) != 1) {
			perror(outfilename);
			return -1;
		}
	}
}

enum KeyType {
	KEY_HEX,
	KEY_STR,
	KEY_FILE,
};

int main(int argc, char* argv[]) {
	struct option options[] = {
		{"help",   no_argument, 0, 'h'},
		{"hex",    no_argument, 0, 'x'},
		{"str",    no_argument, 0, 's'},
		{"file",   no_argument, 0, 'f'},
		{"output", no_argument, 0, 'o'},
		{NULL,     0,           0,  0 },
	};
	enum KeyType key_type = KEY_HEX;
	const char *output = NULL;
	size_t key_size = 0;
	uint8_t *key = NULL;
	int status = 0;
	FILE *outfp = NULL;
	FILE *infp = NULL;
	FILE *keyfp = NULL;
	const char *infilename = NULL;
	uint8_t *buf = NULL;

	for (;;) {
		int opt = getopt_long(argc, argv, "hxsfo:", options, NULL);
		if (opt == -1) {
			break;
		}

		switch (opt) {
			case 'h':
				usage(argc, argv);
				return 0;
			
			case 'x':
				key_type = KEY_HEX;
				break;
			
			case 's':
				key_type = KEY_STR;
				break;
			
			case 'f':
				key_type = KEY_FILE;
				break;

			case 'o':
				output = optarg;
				break;

			case '?':
				fprintf(stderr, "unrecognized argument -%c\n", optopt);
				usage(argc, argv);
				goto error;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "KEY argument is required\n");
		usage(argc, argv);
		goto error;
	}

	const char *keyarg = argv[optind];

	switch (key_type) {
		case KEY_HEX:
			key_size = strlen(keyarg) / 2;
			key = calloc(1, key_size);
			if (!key) {
				perror("allocating memory for key");
				goto error;
			}
			const char *ptr = keyarg;
			size_t index = 0;
			while (*ptr) {
				int byte = parse_hex_byte(ptr);
				if (byte < 0) {
					fprintf(stderr,
						"error parsing key hex string at index %zu\n",
						(size_t)(ptr - keyarg));
					goto error;
				}
				key[index ++] = (uint8_t)byte;
				ptr += 2;
			}
			break;

		case KEY_STR:
			key = (uint8_t*)argv[optind];
			key_size = strlen(keyarg);
			break;

		case KEY_FILE:
			keyfp = fopen(keyarg, "rb");
			if (!keyfp) {
				perror(keyarg);
				goto error;
			}

			struct stat info;
			if (fstat(fileno(keyfp), &info) != 0) {
				perror(keyarg);
				goto error;
			}

			key_size = info.st_size;
			key = calloc(1, key_size);
			if (!key) {
				perror("allocating memory for key");
				goto error;
			}

			if (fread(key, key_size, 1, keyfp) != 1) {
				perror(keyarg);
				goto error;
			}

			if (fclose(keyfp) != 0) {
				perror(keyarg);
				keyfp = NULL;
				goto error;
			}

			break;

		default:
			assert(false);
	}

	if (key_size == 0) {
		fprintf(stderr, "KEY cannot be empty\n");
		goto error;
	}

	buf = calloc(1, key_size);
	if (!buf) {
		perror("allocating buffer");
		goto error;
	}

	++ optind;

	const char *outfilename = output;
	if (output) {
		outfp = fopen(output, "wb");
		if (!outfp) {
			perror(output);
			goto error;
		}
	} else {
		outfp = stdout;
		outfilename = "<stdout>";
	}

	if (optind < argc) {
		for (; optind < argc; ++ optind) {
			infilename = argv[optind];
			infp = fopen(infilename, "rb");

			if (!infp) {
				perror(infilename);
				goto error;
			}

			if (fxor(infp, infilename, outfp, outfilename, key, buf, key_size) != 0) {
				goto error;
			}

			if (fclose(infp) != 0) {
				perror(infilename);
				infp = NULL;
				goto error;
			}

			infp = NULL;
		}
	} else {
		if (fxor(stdin, "<stdin>", outfp, outfilename, key, buf, key_size) != 0) {
			goto error;
		}
	}

	goto end;

error:
	status = 1;

end:

	if (key_type != KEY_STR && key) {
		free(key);
		key = NULL;
	}

	if (keyfp) {
		if (fclose(keyfp) != 0) {
			perror(keyarg);
			status = 1;
		}
		keyfp = NULL;
	}

	if (infp) {
		if (fclose(infp) != 0) {
			perror(infilename);
			status = 1;
		}
		infp = NULL;
	}

	if (output && outfp) {
		if (fclose(outfp) != 0) {
			perror(output);
			status = 1;
		}
		outfp = NULL;
	}

	if (buf) {
		free(buf);
		buf = NULL;
	}

	return status;
}