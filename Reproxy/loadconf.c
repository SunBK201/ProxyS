#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include "loadconf.h"

int parse_server(xmlDocPtr doc, xmlNodePtr cur, struct config *CONF)
{
    assert(doc || cur);
    xmlChar *key;

    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        //获取localport
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"localport")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            CONF->LOCALPORT = atoi(key);
            printf("localport: %d\n", CONF->LOCALPORT);
            xmlFree(key);
        }
        //获取remotehost
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"remotehost")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            CONF->REMOTE_HOST = key;

            printf("remotehost: %s\n", CONF->REMOTE_HOST);
            //xmlFree(key);
        }
        //获取remoteport
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"remoteport")))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            CONF->REMOTE_PORT = atoi(key);
            printf("remoteport: %d\n", CONF->REMOTE_PORT);
            xmlFree(key);
        }
        cur = cur->next;
    }
    return 0;
}

int parse_conf_file(const char *file_name, struct config *CONF)
{
    assert(file_name);

    xmlDocPtr doc;  //xml整个文档的树形结构
    xmlNodePtr cur; //xml节点
    xmlChar *id;    //phone id

    //获取树形结构
    doc = xmlParseFile(file_name);
    if (doc == NULL)
    {
        fprintf(stderr, "Failed to parse xml file:%s\n", file_name);
        goto FAILED;
    }

    //获取根节点
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL)
    {
        fprintf(stderr, "Root is empty.\n");
        goto FAILED;
    }

    if ((xmlStrcmp(cur->name, (const xmlChar *)"Appconf")))
    {
        fprintf(stderr, "The root is not Appconf.\n");
        goto FAILED;
    }

    //遍历处理根节点的每一个子节点
    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)"server")))
        {
            parse_server(doc, cur, CONF);
        }
        cur = cur->next;
    }
    xmlFreeDoc(doc);

    return 0;
FAILED:
    if (doc)
    {
        xmlFreeDoc(doc);
    }
    return -1;
}
