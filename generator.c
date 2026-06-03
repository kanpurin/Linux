#include <stdio.h>
#include <string.h>
#include "testgen.h"

static void sh_single_quote(FILE *fp, const char *s) {
    fputc('\'', fp);
    for (; *s; s++) {
        if (*s == '\'') fputs("'\\''", fp);
        else fputc(*s, fp);
    }
    fputc('\'', fp);
}

int generate_test_script(const TestCase *tc, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "#!/bin/bash\n\nset -u\n\nRESULT=\"\"\n\n");
    fprintf(fp, "# Test: %s\n", tc->name);
    fprintf(fp, "# Procedure: assign the value to test to RESULT.\n");
    for (int i = 0; i < tc->procedure.line_count; i++) {
        fprintf(fp, "%s\n", tc->procedure.lines[i]);
    }
    fprintf(fp, "\n# Assertion\n");
    switch (tc->assertion.type) {
    case ASSERT_EXACT:
        fprintf(fp, "EXPECTED="); sh_single_quote(fp, tc->assertion.expected); fprintf(fp, "\n");
        fprintf(fp, "if [ \"$RESULT\" = \"$EXPECTED\" ]; then\n  echo OK\n  exit 0\nelse\n  echo NG\n  echo \"expected: $EXPECTED\"\n  echo \"actual:   $RESULT\"\n  exit 1\nfi\n");
        break;
    case ASSERT_REGEX:
        fprintf(fp, "REGEX="); sh_single_quote(fp, tc->assertion.expected); fprintf(fp, "\n");
        fprintf(fp, "if printf '%%s\\n' \"$RESULT\" | grep -Eq \"$REGEX\"; then\n  echo OK\n  exit 0\nelse\n  echo NG\n  echo \"regex:  $REGEX\"\n  echo \"actual: $RESULT\"\n  exit 1\nfi\n");
        break;
    case ASSERT_RANGE:
        fprintf(fp, "MIN=%ld\nMAX=%ld\n", tc->assertion.min, tc->assertion.max);
        fprintf(fp, "case \"$RESULT\" in ''|*[!0-9-]*) echo \"NG: RESULT is not an integer: $RESULT\"; exit 1;; esac\n");
        fprintf(fp, "if [ \"$RESULT\" -ge \"$MIN\" ] && [ \"$RESULT\" -le \"$MAX\" ]; then\n  echo OK\n  exit 0\nelse\n  echo NG\n  echo \"range:  $MIN..$MAX\"\n  echo \"actual: $RESULT\"\n  exit 1\nfi\n");
        break;
    case ASSERT_CONTAINS:
        fprintf(fp, "NEEDLE="); sh_single_quote(fp, tc->assertion.expected); fprintf(fp, "\n");
        fprintf(fp, "case \"$RESULT\" in *\"$NEEDLE\"*) echo OK; exit 0;; *) echo NG; echo \"expected to contain: $NEEDLE\"; echo \"actual: $RESULT\"; exit 1;; esac\n");
        break;
    case ASSERT_NOT_CONTAINS:
        fprintf(fp, "NEEDLE="); sh_single_quote(fp, tc->assertion.expected); fprintf(fp, "\n");
        fprintf(fp, "case \"$RESULT\" in *\"$NEEDLE\"*) echo NG; echo \"must not contain: $NEEDLE\"; echo \"actual: $RESULT\"; exit 1;; *) echo OK; exit 0;; esac\n");
        break;
    }
    fclose(fp);
    return 0;
}
