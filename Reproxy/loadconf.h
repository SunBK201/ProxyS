#ifndef __LOADCONF_H_
#define __LOADCONF_H_

struct config
{
    int LOCALPORT;
    char *REMOTE_HOST;
    int REMOTE_PORT;
};

int parse_conf_file(const char *file_name, struct config *CONF);

#endif