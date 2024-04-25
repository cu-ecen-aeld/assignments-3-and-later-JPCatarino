// # Author: Jorge Catarino

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);

    if (argc < 3) {
        syslog(LOG_ERR, "too few arguments");
        closelog();
        return 1;
    }

    char *writefile = argv[1];
    char *writestr = argv[2];

    FILE *stream = fopen(writefile, "w");
    if (!stream) {
        syslog(LOG_ERR, "could not open file %s", writefile);
        closelog();
        return 1;
    }

    if (fputs (writestr, stream) == EOF){
        syslog(LOG_ERR, "could not write to file %s", writefile);
        fclose(stream);
        closelog();
        return 1;
    }

    fclose(stream);

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    closelog();
    return 0;
}