#pragma once
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include <Epub.h>
#include <dirent.h>
#include "esp_log.h"
#include "renderer.h"
#include <map>
#include "bookHandler.h"
#include <memory>
#include <Epub.h>

class BookMarkMenuHandler
{
    public:
        BookMarkMenuHandler(Book *book, Renderer *renderer,Epub *epub,unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer);
        ~BookMarkMenuHandler();


        enum class MenuElementID {
            Return,
            NextChapt,
            PrevChapt,
            AddMark,
            GotoMark,
            ToggleNightMode,
            ToggleSunlightMode,
            pageDisplay,
            GotoChapter
        };

        struct MenuElement 
        {
            MenuElementID ID;
            std::string text;
        };

        std::vector<MenuElement> menuElements = {
            MenuElement(MenuElementID::Return,"Return to book"),
            MenuElement(MenuElementID::NextChapt,"Next chapter"),
            MenuElement(MenuElementID::GotoChapter,"Go to chapter"),
            MenuElement(MenuElementID::PrevChapt,"Previous chapter"),
            MenuElement(MenuElementID::ToggleNightMode,"Toggle Dark mode"),
            MenuElement(MenuElementID::ToggleSunlightMode,"Sunlight mode"),
            MenuElement(MenuElementID::pageDisplay,"Display pagecount"),
            MenuElement(MenuElementID::AddMark,"Add bookmark"),
            MenuElement(MenuElementID::GotoMark,"Go to bookmark"),
            
        };

        int currentTocIndex = 0;
        int currentMenuElementIndex = 0;
        int currentVerticalElementIndex = 0;
        int maxVerticalElements = 0;
        bool bookMarkOnPage = false;


        void drawMenu();

        Renderer *renderer;
        Book *book;
        Epub *epub;
        int selectedBookMarkIndex = -1;

        unsigned char* preservedFramebuffer;
        unsigned char* leftPageFrameBuffer;
        unsigned char* rightPageFrameBuffer;
    private:

};