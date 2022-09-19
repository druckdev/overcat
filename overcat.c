#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROG_NAME "overcat"
#define BUF_SIZE 4096

#define CLEANUP_MAIN() do { \
		for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) \
			if (files[i]) \
				fclose(files[i]); \
		if (out != stdout) \
			fclose(out); \
	} while(0);

/*
 * Print the program's usage to stdout.
 */
void
print_usage()
{
	printf("Usage: %s [OPTION]... FILE...\n", PROG_NAME);
	printf("Concatenates overlapping files, removing the redundant overlaps.\n");
	printf("\n");
	printf("  -h, --help           Print the help message and exit.\n");
	printf("  -o, --output=FILE    Write output to FILE instead of stdout.\n");
	printf("  -v, --verbose        Enable verbose log messages on stderr.\n");
}

/*
 * Open a file and log error messages if necessary.
 */
FILE*
open_file(const char* path, const char* mode)
{
	FILE* file;
	if (!(file = fopen(path, mode))) {
		fprintf(stderr, "%s: couldn't open '%s':\n", PROG_NAME, path);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return file;
}

/*
 * Write bytes from one file to another. Does not modify the file's positions
 * beforehand.
 */
int
write_from_to(FILE* from, FILE* to)
{
	if (!from || !to)
		return 0;

	unsigned char buf[BUF_SIZE];
	size_t n_read;
	while ((n_read = fread(buf, sizeof(*buf), sizeof(buf), from))) {
		fwrite(buf, sizeof(*buf), n_read, to);
		// TODO: error handling
	}

	return 1;
}

int
main(int argc, char* argv[])
{
	// Defaults that can be overridden by some flags
	FILE* out = stdout;
	int verbose = 0;

	// NOTE: When changing something here, remember to update `print_usage`.
	char shortopts[] = "ho:v";
	struct option longopts[] = {
		// const char *name, int has_arg, int *flag, int val
		{"help",    0, NULL, 'h'},
		{"output",  1, NULL, 'o'},
		{"verbose", 0, NULL, 'v'},
		{0} // The last element has to be filled with zeros.
	};

	// Flag handling
	int opt;
	while (-1 != (opt = getopt_long(argc, argv, shortopts, longopts, NULL))) {
		switch (opt) {
		case 'h':
			print_usage();
			if (out != stdout)
				fclose(out);
			return EXIT_SUCCESS;
		case 'o':
			if (!(out = open_file(optarg, "w")))
				return EXIT_FAILURE;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "%s: unrecognized option '%s'\n", PROG_NAME, argv[optind]);
			fprintf(stderr, "Try '%s --help' for more information.\n", PROG_NAME);
			if (out != stdout)
				fclose(out);
			return EXIT_FAILURE;
		}
	}

	if (argc - optind <= 0) {
		print_usage();
		return EXIT_FAILURE;
	}

	FILE* files[argc - optind];
	// Null `files`, so that CLEANUP_MAIN knows which files were opened already
	memset(files, 0, sizeof(files));
	// Open all files before starting to read
	for (size_t i = 0; i < sizeof(files)/sizeof(*files); ++i) {
		if (!(files[i] = open_file(argv[optind + i], "r"))) {
			CLEANUP_MAIN();
			return EXIT_FAILURE;
		}
	}

	// Write first file completely
	if (!write_from_to(files[0], out)) {
		CLEANUP_MAIN();
		return EXIT_FAILURE;
	}

	int ret = EXIT_SUCCESS;
	for (size_t i = 1; i < sizeof(files)/sizeof(*files); ++i) {
		size_t len = 0;

		// Read last byte of previous file
		fseek(files[i-1], -1, SEEK_END);
		unsigned char last = fgetc(files[i-1]);

		int cur;
		while (1) {
			// Make sure we read new bytes
			fseek(files[i], len, SEEK_SET);

			// Search for `last` in current file
			do {
				cur = fgetc(files[i]);
				++len;
			} while (cur != last && cur != EOF);

			if (cur == EOF) {
				// No overlap
				len = 0;
				break;
			}

			// Don't bother with the buffers when the overlap is only one byte
			// small.
			if (len == 1)
				break;

			// Read last/first `len` bytes, without the already matching `last`
			// TODO: Use chunks of 4KB instead of `len - 1`
			// TODO: Use mmap(2) with fread as fallback if the files are
			//       too big?
			unsigned char buf1[len - 1];
			unsigned char buf2[len - 1];
			fseek(files[i-1], -len, SEEK_END);
			rewind(files[i]);
			// TODO: error handling
			fread(buf1, sizeof(*buf1), sizeof(buf1), files[i-1]);
			fread(buf2, sizeof(*buf2), sizeof(buf2), files[i]);

			// Compare last bytes of previous file with first bytes of current
			// file
			if (!memcmp(buf1, buf2, sizeof(buf1)))
				// memory sections match, overlap found
				break;
		}

		if (verbose)
			fprintf(stderr, "%s: Overlap of %lu bytes.\n", PROG_NAME, len);

		// Write the rest
		fseek(files[i], len, SEEK_SET);
		if ((ret = !write_from_to(files[i], out)))
			break;
	}

	CLEANUP_MAIN();
	return ret;
}
