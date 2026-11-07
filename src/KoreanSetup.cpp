/*
 * Copyright 2026 rainygirl. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * koreanime-setup -- makes Korean the system language, if it is not.
 *
 * The package runs it once after installation. It changes the preferred
 * language and the formatting conventions (ko, ko_KR) through
 * MutableLocaleRoster, as the Locale preferences do, so that running
 * applications switch to Korean at once. A system that already uses Korean
 * is left exactly as it is. The time zone is not touched.
 */

#include <Application.h>
#include <FormattingConventions.h>
#include <Locale.h>
#include <LocaleRoster.h>
#include <Message.h>
#include <MutableLocaleRoster.h>
#include <String.h>

#include <stdio.h>
#include <string.h>

using BPrivate::MutableLocaleRoster;

static const char* kAppSignature = "application/x-vnd.rainygirl-koreanime-setup";


static bool
IsKorean(const char* id)
{
	return id != NULL && strncmp(id, "ko", 2) == 0
		&& (id[2] == '\0' || id[2] == '_' || id[2] == '-');
}


static BString
PreferredLanguage()
{
	BMessage languages;
	BString first;
	if (BLocaleRoster::Default()->GetPreferredLanguages(&languages) == B_OK)
		languages.FindString("language", 0, &first);
	return first;
}


static BString
Conventions()
{
	BFormattingConventions conventions;
	if (BLocale::Default()->GetFormattingConventions(&conventions) == B_OK)
		return BString(conventions.ID());
	return BString();
}


static status_t
SetLanguage(const char* language, const char* conventionsID)
{
	BMessage languages;
	status_t status = languages.AddString("language", language);
	if (status == B_OK)
		status = MutableLocaleRoster::Default()->SetPreferredLanguages(
			&languages);
	if (status != B_OK)
		return status;

	BFormattingConventions conventions(conventionsID);
	return MutableLocaleRoster::Default()->SetDefaultFormattingConventions(
		conventions);
}


static void
PrintStatus()
{
	printf("language       %s\n", PreferredLanguage().String());
	printf("formatting     %s\n", Conventions().String());
}


static void
Usage(const char* name)
{
	printf("usage: %s [--status | --english]\n"
		"  (none)      switch the language to Korean (ko, ko_KR) unless it\n"
		"              already is Korean\n"
		"  --status    show the current settings\n"
		"  --english   switch back to English (en, en_US)\n", name);
}


int
main(int argc, char** argv)
{
	bool status = false;
	bool english = false;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--status") == 0)
			status = true;
		else if (strcmp(argv[i], "--english") == 0)
			english = true;
		else {
			Usage(argv[0]);
			return strcmp(argv[i], "--help") == 0 ? 0 : 1;
		}
	}

	// the roster broadcasts the change from a running BApplication
	status_t result;
	BApplication app(kAppSignature, &result);
	if (result != B_OK) {
		fprintf(stderr, "koreanime-setup: no registrar yet (%s)\n",
			strerror(result));
		return 2;
	}

	if (status) {
		PrintStatus();
		return 0;
	}

	if (english) {
		result = SetLanguage("en", "en_US");
	} else {
		if (IsKorean(PreferredLanguage().String())
			&& IsKorean(Conventions().String())) {
			PrintStatus();
			return 0;
		}
		result = SetLanguage("ko", "ko_KR");
	}

	if (result != B_OK) {
		fprintf(stderr, "koreanime-setup: %s\n", strerror(result));
		return 1;
	}
	PrintStatus();
	return 0;
}
