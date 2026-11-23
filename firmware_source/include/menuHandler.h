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
#include "UIElements.h"

class MenuHandler
{
    public:
        MenuHandler(Renderer *renderer);

        void middleButtonAction();
        void rightButtonAction();
        void leftButtonAction();
        void upButtonAction();
        void downButtonAction();
        void rightPageAction();
        void leftPageAction();

        void SetBookElementActive(Book* book);
        void layoutLibraryMenu();
        void updateSettingsValues();
        void layoutFontSelect();
        void updateFontSize();
        void updateFontSelect();
        void updateFont();
        
        void drawMenu();
        void drawValuePartial();
        bool buzzDisabled = false;
        std::function<void()> displayIdleCallbackReturn;

        Renderer *renderer;
        std::shared_ptr<UIElement> currentElement;
        std::shared_ptr<MenuElement> mainMenu;
        std::shared_ptr<MenuElement> settingsMenu;
        std::shared_ptr<MenuElement> deviceSettingsMenu;
        std::shared_ptr<MenuElement> readSettingsMenu;
        std::shared_ptr<MenuElement> einkSettingsMenu;
        std::shared_ptr<MenuElement> libraryMenu; 
        std::shared_ptr<HorizontalMenuElement> librarySubMenuElement; 
        std::shared_ptr<ValueElement> fontSelectBox; 
        std::shared_ptr<ValueElement> fontSizeBox; 
        std::shared_ptr<FavoritesFolderElement> favoriteBooksFolder; 

        unsigned char* leftPageFrameBuffer;
        unsigned char* rightPageFrameBuffer;
    private:

};