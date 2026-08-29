#pragma once
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include "htmlParser.h"
#include <Epub.h>
#include <dirent.h>
#include "esp_log.h"
#include "renderer.h"
#include <map>
#include <memory>

class NotificationHandler
{
    public:
        NotificationHandler(Renderer *renderer);

        
        void drawUSBQuery();
        void drawManualFileTransfer(bool disconnect);
        void drawIndexingNotification(std::string bookTitle);
        void drawIndexingNotification(std::string bookTitle,int percent);
        void drawBookOpeningNotification(std::string bookTitle);
        void drawNotification(std::string notificationText);
        void drawErrorNotification(std::string bookTitle);
        void drawSDcardErrorNotification();
        void drawStorageAccessNotication();
        Renderer *renderer;
        
    private:

};