/*
 * kas.c  blockfiler (.kas) för ABC-kassetten: lista, packa och packa upp.
 *
 *   kas lista BAND.kas                 filerna och blocken på bandet
 *   kas packa -o BAND.kas FIL ...      filer -> block (.BAS/.TXT blir text)
 *   kas upp [-d KATALOG] BAND.kas      block -> filer
 *
 * Vid packning anger -t (text) eller -b (binär) typen, och --tk N
 * tidkonstanten för ABC 800. Samma som python/kas.py.
 *
 * S. Lönnert 2026. Public domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "abckas.h"

static void usage(void)
{
    fprintf(stderr,
            "användning: kas lista BAND.kas\n"
            "            kas packa -o BAND.kas [-t | -b] [--tk N] FIL ...\n"
            "            kas upp [-d KATALOG] BAND.kas\n");
    exit(2);
}

static void print_missing(FILE *fp, const char *prefix, const TapeFile *f)
{
    long n = file_missing(f, NULL), i;
    long *m;
    if (!n)
        return;
    m = (long *) malloc((size_t) n * sizeof(long));
    file_missing(f, m);
    fputs(prefix, fp);
    for (i = 0; i < n; i++)
        fprintf(fp, i ? ",%ld" : "%ld", m[i]);
    free(m);
}

static int lista(const char *band)
{
    Blocks bl;
    TapeFile *f;
    long nf, i;
    char name[64];

    blocks_init(&bl);
    if (read_kas(band, &bl))
        return 1;
    f = group_files(&bl, &nf);
    printf("%s: %ld block, %ld fil(er)\n", band, bl.n, nf);
    for (i = 0; i < nf; i++) {
        file_name(&f[i], name);
        fputs("  ", stdout);
        print_padded(name[0] ? name : "(namn saknas)", 12);
        printf(" %-4s %4ld block", kind_name(file_kind(&f[i])), f[i].count);
        if (f[i].header && f[i].header[14])
            printf(" tk %d", f[i].header[14]);
        print_missing(stdout, "  saknas: ", &f[i]);
        putchar('\n');
    }
    free_files(f, nf);
    blocks_free(&bl);
    return 0;
}

static int packa(int argc, char **argv)
{
    const char *out = NULL;
    int kind = KIND_GUESS, tk = -1, i, nfiles = 0;
    char **files = (char **) malloc((size_t) argc * sizeof(char *));
    Blocks bl;
    int rc;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if (++i >= argc)
                usage();
            out = argv[i];
        } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--text")) {
            kind = KIND_TEXT;
        } else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--binar")) {
            kind = KIND_BIN;
        } else if (!strcmp(argv[i], "--tk")) {
            if (++i >= argc)
                usage();
            tk = atoi(argv[i]);
        } else if (argv[i][0] == '-' && argv[i][1]) {
            usage();
        } else {
            files[nfiles++] = argv[i];
        }
    }
    if (!out || !nfiles)
        usage();
    blocks_init(&bl);
    for (i = 0; i < nfiles; i++) {
        long len, before = bl.n;
        char name[64];
        byte *d = read_file(files[i], &len);
        if (!d) {
            perror(files[i]);
            return 1;
        }
        make_file(&bl, files[i], d, len, kind, tk);
        show_name(bl.b[before] + 3, name);
        fprintf(stderr, "%s: %s, %ld datablock\n", files[i], name, bl.n - before - 1);
        free(d);
    }
    rc = write_kas(out, &bl);
    blocks_free(&bl);
    free(files);
    return rc;
}

static int upp(int argc, char **argv)
{
    const char *dir = "ut", *band = NULL;
    Blocks bl;
    TapeFile *f;
    long nf, i;
    int i2, fel = 0;

    for (i2 = 0; i2 < argc; i2++) {
        if (!strcmp(argv[i2], "-d") || !strcmp(argv[i2], "--katalog")) {
            if (++i2 >= argc)
                usage();
            dir = argv[i2];
        } else if (argv[i2][0] == '-' && argv[i2][1]) {
            usage();
        } else if (!band) {
            band = argv[i2];
        } else {
            usage();
        }
    }
    if (!band)
        usage();
    blocks_init(&bl);
    if (read_kas(band, &bl))
        return 1;
    mkdir(dir, 0777);
    f = group_files(&bl, &nf);
    for (i = 0; i < nf; i++) {
        char name[64], *path, *s;
        byte *d;
        long len;
        FILE *fp;
        file_name(&f[i], name);
        if (!name[0])
            sprintf(name, "NAMNLOS%ld", i);
        for (s = name; *s; s++)
            if (*s == '/')
                *s = '_';
        path = (char *) malloc(strlen(dir) + strlen(name) + 2);
        sprintf(path, "%s/%s", dir, name);
        d = file_content(&f[i], &len);
        fp = fopen(path, "wb");
        if (!fp) {
            perror(path);
            return 1;
        }
        fwrite(d, 1, (size_t) len, fp);
        fclose(fp);
        fprintf(stderr, "%s: %s", path, kind_name(file_kind(&f[i])));
        if (file_missing(&f[i], NULL))
            fel = 1;
        print_missing(stderr, "  saknar block ", &f[i]);
        fputc('\n', stderr);
        free(d);
        free(path);
    }
    free_files(f, nf);
    blocks_free(&bl);
    return fel;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        usage();
    if (!strcmp(argv[1], "lista") && argc == 3)
        return lista(argv[2]);
    if (!strcmp(argv[1], "packa"))
        return packa(argc - 2, argv + 2);
    if (!strcmp(argv[1], "upp"))
        return upp(argc - 2, argv + 2);
    usage();
    return 2;
}
