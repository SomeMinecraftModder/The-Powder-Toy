#ifdef NEWINTERFACE
#ifndef PROFILEVIEWER_H
#define PROFILEVIEWER_H

#include <string>
#include "interface/Window.h"
#include "graphics.h"

class Download;
class Label;
class ProfileViewer : public Window_
{
	std::string name;
	Download *profileInfoDownload;
	Download *avatarDownload;
	pixel *avatar;

	Label *usernameLabel, *ageLabel, *websiteLabel, *locationLabel, *biographyLabel;
	Label *saveCountLabel, *saveAverageLabel, *highestVoteLabel;
	void MainLoop();
public:
	ProfileViewer(std::string profileName);
	~ProfileViewer();

	void OnTick(float dt);
	void OnDraw(VideoBuffer *buf);
};

#endif
#endif