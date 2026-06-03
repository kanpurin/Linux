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

static const char *assert_name(AssertType type) {
    switch (type) {
        case ASSERT_EXACT: return "exact";
        case ASSERT_REGEX: return "regex";
        case ASSERT_RANGE: return "range";
        case ASSERT_CONTAINS: return "contains";
        case ASSERT_NOT_CONTAINS: return "not_contains";
        default: return "unknown";
    }
}

int generate_test_script(const TestCase *tc, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "#!/bin/bash\n\n");
    fprintf(fp, "set -u\n\n");
    fprintf(fp, "RESULT=\"\"\n\n");
    fprintf(fp, "# Test: %s\n", tc->name);
    fprintf(fp, "# Assertion: %s\n\n", assert_name(tc->assertion.type));

    fprintf(fp, "# procedure\n");
    for (int i = 0; i < tc->procedure.line_count; i++) {
        fprintf(fp, "%s\n", tc->procedure.lines[i]);
    }

    fprintf(fp, "\n# assertion\n");
    switch (tc->assertion.type) {
        case ASSERT_EXACT:
            fprintf(fp, "EXPECTED=");
            sh_single_quote(fp, tc->assertion.expected);
            fprintf(fp, "\n");
            fprintf(fp, "if [ \"$RESULT\" = \"$EXPECTED\" ]; then\n");
            break;
        case ASSERT_REGEX:
            fprintf(fp, "REGEX=");
            sh_single_quote(fp, tc->assertion.expected);
            fprintf(fp, "\n");
            fprintf(fp, "if printf '%%s\\n' \"$RESULT\" | grep -Eq -- \"$REGEX\"; then\n");
            break;
        case ASSERT_RANGE:
            fprintf(fp, "MIN=%ld\n", tc->assertion.min);
            fprintf(fp, "MAX=%ld\n", tc->assertion.max);
            fprintf(fp, "if [[ \"$RESULT\" =~ ^-?[0-9]+$ ]] && [ \"$RESULT\" -ge \"$MIN\" ] && [ \"$RESULT\" -le \"$MAX\" ]; then\n");
            break;
        case ASSERT_CONTAINS:
            fprintf(fp, "EXPECTED=");
            sh_single_quote(fp, tc->assertion.expected);
            fprintf(fp, "\n");
            fprintf(fp, "if [[ \"$RESULT\" == *\"$EXPECTED\"* ]]; then\n");
            break;
        case ASSERT_NOT_CONTAINS:
            fprintf(fp, "EXPECTED=");
            sh_single_quote(fp, tc->assertion.expected);
            fprintf(fp, "\n");
            fprintf(fp, "if [[ \"$RESULT\" != *\"$EXPECTED\"* ]]; then\n");
            break;
        default:
            fclose(fp);
            return -1;
    }

    fprintf(fp, "    echo \"OK\"\n");
    fprintf(fp, "    exit 0\n");
    fprintf(fp, "else\n");
    fprintf(fp, "    echo \"NG\"\n");
    fprintf(fp, "    echo \"RESULT=$RESULT\"\n");
    if (tc->assertion.type == ASSERT_RANGE) {
        fprintf(fp, "    echo \"expected range: $MIN..$MAX\"\n");
    } else if (tc->assertion.type == ASSERT_REGEX) {
        fprintf(fp, "    echo \"expected regex: $REGEX\"\n");
    } else {
        fprintf(fp, "    echo \"expected: $EXPECTED\"\n");
    }
    fprintf(fp, "    exit 1\n");
    fprintf(fp, "fi\n");

    fclose(fp);
    return 0;
}
