/*
 * PROJECT:         ReactOS Downloader
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            base\applications\downloader\xml.c
 * PURPOSE:         Parsing of application information xml files
 * PROGRAMMERS:     Maarten Bosma
 */

#include <libs/expat/expat.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "structures.h"

BOOL TagOpen;
struct Category* Current;
struct Application* CurrentApplication;
char CurrentTag [0x100];

void tag_opened (void* usrdata, const char* tag, const char** arg)
{
	if(!strcmp(tag, "tree") && !CurrentApplication)
	{
		// check version
	}

	else if(!strcmp(tag, "category") && !CurrentApplication)
	{
		if (!Current)
		{
			Current = malloc(sizeof(struct Category));
			memset(Current, 0, sizeof(struct Category));
		}
		else if (TagOpen)
		{
			Current->Children = malloc(sizeof(struct Category));
			memset(Current->Children, 0, sizeof(struct Category));
			Current->Children->Parent = Current;
			Current = Current->Children;
		}
		else
		{
			Current->Next = malloc(sizeof(struct Category));
			memset(Current->Next, 0, sizeof(struct Category));
			Current->Next->Parent = Current->Parent;
			Current = Current->Next;
		}
		TagOpen = TRUE;

		int i;
		for (i=0; arg[i]; i+=2) 
		{
			if(!strcmp(arg[i], "name"))
			{
				MultiByteToWideChar(CP_ACP, 0, arg[i+1], -1, Current->Name, 0x100);
			}
			if(!strcmp(arg[i], "icon"))
			{
				Current->Icon = atoi(arg[i+1]);
			}
		}
	}

	else if(!strcmp(tag, "application") && !CurrentApplication)
	{
		if(Current->Apps)
		{
			CurrentApplication = Current->Apps;
			while(CurrentApplication->Next)
				CurrentApplication = CurrentApplication->Next;
			CurrentApplication->Next = malloc(sizeof(struct Application));
			memset(CurrentApplication->Next, 0, sizeof(struct Application));
			CurrentApplication = CurrentApplication->Next;
		}
		else
		{
			Current->Apps = malloc(sizeof(struct Application));
			memset(Current->Apps, 0, sizeof(struct Application));
			CurrentApplication = Current->Apps;
		}

		int i;
		for (i=0; arg[i]; i+=2) 
		{
			if(!strcmp(arg[i], "name"))
			{
				MultiByteToWideChar(CP_ACP, 0, arg[i+1], -1, CurrentApplication->Name, 0x100);
			}
		}
	}
	else if (CurrentApplication)
	{
		strncpy(CurrentTag, tag, 0x100);
	}
	else
		MessageBox(0,L"Invaild XML File1",0,0);
}


void text (void* usrdata, const char* data, int len)
{
	if (!CurrentApplication)
		return;

	// FIXME: handle newlines e.g. in Description
	if(!strcmp(CurrentTag, "maintainer"))
	{
		int currentlengt = lstrlen(CurrentApplication->Location);
		MultiByteToWideChar(CP_ACP, 0, data, len, &CurrentApplication->Maintainer[currentlengt], 0x100-currentlengt);
	}
	else if(!strcmp(CurrentTag, "description"))
	{
		int currentlengt = lstrlen(CurrentApplication->Description);
		MultiByteToWideChar(CP_ACP, 0, data, len, &CurrentApplication->Description[currentlengt], 0x400-currentlengt);
	}
	else if(!strcmp(CurrentTag, "location"))
	{
		int currentlengt = lstrlen(CurrentApplication->Location);
		MultiByteToWideChar(CP_ACP, 0, data, len, &CurrentApplication->Location[currentlengt], 0x100-currentlengt);
	}
}

void tag_closed (void* tree, const char* tag)
{
	CurrentTag[0] = 0;

	if(!strcmp(tag, "category"))
	{
		if (TagOpen)
		{
			TagOpen = FALSE;
		}
		else
		{
			Current = Current->Parent;
		}
	}
	else if(!strcmp(tag, "application"))
	{
		CurrentApplication = NULL;
	}
}

BOOL ProcessXML (const char* filename, struct Category* Root) 
{
	int done = 0;
	char buffer[255];

	if(Current)
		return FALSE;

	Current = Root;
	TagOpen = TRUE;

	FILE* file = fopen(filename, "r");
	if(!file) 
	{
		MessageBox(0,L"Could not find the xml file !",0,0);
		return FALSE;
	}

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetElementHandler(parser, tag_opened, tag_closed);
	XML_SetCharacterDataHandler(parser, text);

	while (!done)
	{
		size_t len = fread (buffer, 1, sizeof(buffer), file);
		done = len < sizeof(buffer);

		buffer[len] = 0;
		if(!XML_Parse(parser, buffer, len, done)) 
		{
			MessageBox(0,L"Could not parse the xml file !",0,0);
			return FALSE;
		}
	}

	XML_ParserFree(parser);
	fclose(file);

	return TRUE;
}

void FreeApps (struct Application* Apps)
{
	if (Apps->Next)
		FreeApps(Apps->Next);

	free(Apps);
}

void FreeTree (struct Category* Node)
{
	if (Node->Children)
		FreeTree(Node->Children);

	if (Node->Next)
		FreeTree(Node->Next);

	if (Node->Apps)
		FreeApps(Node->Apps);

	free(Node);
}
