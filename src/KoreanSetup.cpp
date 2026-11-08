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
 *
 * --hold-add-ons suspends input_server's add-on monitor thread (the shell's
 * kill takes teams, not threads) and prints input_server's team, for the
 * post-install script: see package/post-install.sh.
 */

#include <Application.h>
#include <FormattingConventions.h>
#include <Input.h>
#include <Locale.h>
#include <LocaleRoster.h>
#include <Message.h>
#include <MutableLocaleRoster.h>
#include <String.h>

#include <OS.h>

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


static const char* const kInputServerPath = "/boot/system/servers/input_server";


/*!	Gets input_server ready to be killed: suspends its "AddOnMonitor"
	thread, which unloads an add-on whose file goes away, and prints its
	team. The pointing devices are stopped first, so input_server closes the
	mouse itself: a team killed while its mouse thread is blocked reading the
	PS/2 mouse can leave the mouse open in the kernel, and the next
	input_server then cannot open it -- the pointer is frozen until a reboot.
*/
static int
HoldAddOns()
{
	team_info team;
	int32 teamCookie = 0;
	while (get_next_team_info(&teamCookie, &team) == B_OK) {
		if (strncmp(team.args, kInputServerPath, strlen(kInputServerPath)) != 0)
			continue;
		thread_info thread;
		int32 threadCookie = 0;
		while (get_next_thread_info(team.team, &threadCookie, &thread) == B_OK) {
			if (strcmp(thread.name, "AddOnMonitor") != 0)
				continue;
			BInputDevice::Stop(B_POINTING_DEVICE);
			if (suspend_thread(thread.thread) != B_OK)
				return 1;
			printf("%" B_PRId32 "\n", team.team);
			return 0;
		}
	}
	return 1;
}


static void
Usage(const char* name)
{
	printf("usage: %s [--status | --english | --hold-add-ons]\n"
		"  (none)      switch the language to Korean (ko, ko_KR) unless it\n"
		"              already is Korean\n"
		"  --status    show the current settings\n"
		"  --english   switch back to English (en, en_US)\n"
		"  --hold-add-ons  suspend input_server's add-on monitor, print its\n"
		"              team (used while installing)\n", name);
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
		else if (strcmp(argv[i], "--hold-add-ons") == 0)
			return HoldAddOns();
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
