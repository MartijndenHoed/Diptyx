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
#include <functional>
#include "bookHandler.h"

#define MaxElementsPerPage 6
#define maxMenuheight 28*GLYPH_HEIGHT

enum class UIElementType {
    Menu,
    Value,
    Book,
    Folder,
    Action,
    DropdownSub,
    HorizontalAction,
    HorizontalMenu,
    Unknown
};

class UIElement : public std::enable_shared_from_this<UIElement> {
public:
    // UIElement(Renderer* renderer,std::string name = "", std::string desc = "")
    //     : renderer(renderer), elementName(std::move(name)), elementDescription(std::move(desc)) {}

    UIElement(Renderer* renderer,std::string name = "", std::string desc = "",std::string extraDesc="")
: renderer(renderer), elementName(std::move(name)), elementDescription(std::move(desc)), elementExtraDescription(std::move(extraDesc)) {}

    void setParent(std::shared_ptr<UIElement> p) { parent = p; }
    std::shared_ptr<UIElement> getParent() const { return parent.lock(); }

    Renderer *renderer;

    //virtual void enterElement() = 0;

    //virtual void renderElement() = 0;
    virtual UIElementType getType() const = 0;
    virtual int getElementHeight() const { return 5*GLYPH_HEIGHT; }
    void renderElement();
    virtual void renderIcon(int y,bool Highlight);

    std::string elementName;
    std::string elementDescription;
    std::string elementExtraDescription;

protected:
    std::weak_ptr<UIElement> parent; // weak to avoid cyclic ownership
};

class UIHorizontalElement : public std::enable_shared_from_this<UIHorizontalElement> {
public:
    // UIElement(Renderer* renderer,std::string name = "", std::string desc = "")
    //     : renderer(renderer), elementName(std::move(name)), elementDescription(std::move(desc)) {}

    UIHorizontalElement(Renderer* renderer,std::string name = "")
: renderer(renderer), elementName(std::move(name)) {}

    void setParent(std::shared_ptr<UIElement> p) { parent = p; }
    std::shared_ptr<UIElement> getParent() const { return parent.lock(); }

    Renderer *renderer;

    //virtual void enterElement() = 0;

    //virtual void renderElement() = 0;
    virtual UIElementType getType() const = 0;
    virtual void renderIcon(int y, int centerX, bool Highlight);
    virtual int getElementWidth() const { return 0; }
    std::string elementName;

protected:
    std::weak_ptr<UIElement> parent; // weak to avoid cyclic ownership
};


class MenuElement : public UIElement {

    public:
    using UIElement::UIElement;
    void addChild(std::shared_ptr<UIElement> child) {
        child->setParent(shared_from_this());
        children.push_back(child);
    }

    void renderElement();
    void renderIcon(int y,bool Highlight) override;
    UIElementType getType() const override { return UIElementType::Menu; }

    std::vector<std::shared_ptr<UIElement>> children;
    int selectedChildIndex = 0;
    bool firstElementOnPage = false;
    bool lastElementOnPage = false;
    private:

};

class HorizontalMenuElement : public UIElement {

    public:
    using UIElement::UIElement;
    void addChild(std::shared_ptr<UIHorizontalElement> child) {
        child->setParent(shared_from_this());
        children.push_back(child);
    }

    //void renderElement();
    void renderIcon(int y, bool Highlight) override;
    UIElementType getType() const override { return UIElementType::HorizontalMenu; }
    int getElementHeight() const override { return GLYPH_HEIGHT; }

    std::vector<std::shared_ptr<UIHorizontalElement>> children;
    int selectedChildIndex = 0;
    private:

};

class ValueElement : public UIElement {

    public:
    ValueElement(Renderer* renderer,std::string name, std::string desc, std::vector<int> vals,std::vector<std::string> valDescriptions, int *valueAdress)
        : UIElement(renderer,std::move(name), std::move(desc)), values(std::move(vals)),valueDescriptions(std::move(valDescriptions)),valueAdress(valueAdress) {}
    UIElementType getType() const override { return UIElementType::Value; }

    bool selected = false;
    void renderIcon(int y,bool Highlight) override;
    void writeValue();
    void ReadValue();
    int selectedValueIndex = 0;
    std::vector<int> values;
    std::vector<std::string> valueDescriptions;
    int *valueAdress;
    bool infinitescrolling = false;
    int maxStringWidth = std::max_element(valueDescriptions.begin(), valueDescriptions.end(),
                     [](const std::string& a, const std::string& b) {
                         return a.size() < b.size();
                     })->size() + 4; //width of the longest string +4
    int drawValueXPos = 0;
    int drawValueYPos = 0; //these are determined dynamically, but we need these for drawing the values with partial updates..

    private:

};

class BookElement : public UIElement {

public:
    BookElement(Renderer* renderer, Book* book)
        : UIElement(
            renderer,
            book ? book->title : "",
            book ? std::to_string(book->currentPage + 1) + "/" +
                (book->totalPageCount > 0
                    ? std::to_string(book->totalPageCount) : "?") : "",
            book && book->favorite ? "❤" : ""
        ),
        book(book) {}

    Book* book;
    UIElementType getType() const override { return UIElementType::Book; }
    void renderIcon(int y, bool Highlight) override;
};

class RecentBookElement : public BookElement {

public:
    RecentBookElement(Renderer* renderer)
        : BookElement(renderer, nullptr) {}

    void renderIcon(int y, bool Highlight) override;
};

class FolderElement : public MenuElement {
public:
    FolderElement(Renderer* renderer, Folder* folder)
    : MenuElement(renderer,
                  folder ? folder->name : "Favorites",
                  folder ? "Books: " + std::to_string(folder->bookList.size()) : "Books: 0"),
      folder(folder)
    {}

    void initChildren() {
        children.clear();
        for (Book* book : folder->bookList) {
            auto bookElement = std::make_shared<BookElement>(renderer, book);
            addChild(bookElement);
        }
    }

    
    UIElementType getType() const override { return UIElementType::Folder; }
    Folder* folder;
};

class FavoritesFolderElement : public FolderElement {
public:
    FavoritesFolderElement(Renderer* renderer)
        : FolderElement(renderer, nullptr)
    {}

    void initChildren();
};


class ActionElement : public UIElement {

public:
    using ActionCallback = std::function<void()>;

    ActionElement(Renderer* renderer,
                  std::string name,
                  std::string desc,
                  ActionCallback callback,
                  std::string extraDesc = "")
        : UIElement(renderer,
                    std::move(name),
                    std::move(desc),
                    std::move(extraDesc)),
          action(std::move(callback))
    {}

    UIElementType getType() const override {
        return UIElementType::Action;
    }


    void trigger() {
        ESP_LOGI("UIElements", "Action called");
        if (action) {
            action();
        }
    }

private:
    ActionCallback action;
};

class DropdownElement : public UIHorizontalElement {

    public:
    using ActionCallback = std::function<void()>;
    DropdownElement(Renderer* renderer,std::string name, std::vector<int> vals,std::vector<std::string> valDescriptions, int *valueAdress,ActionCallback callback)
        : UIHorizontalElement(renderer,std::move(name)), values(std::move(vals)),valueDescriptions(std::move(valDescriptions)),valueAdress(valueAdress),action(std::move(callback)) {}
    UIElementType getType() const override { return UIElementType::DropdownSub; }

    bool selected = false;
    void renderIcon(int y, int centerX,bool Highlight) override;
    void writeValue();
    void ReadValue();
    int selectedValueIndex = 0;
    std::vector<int> values;
    std::vector<std::string> valueDescriptions;
    int *valueAdress;
    int maxStringWidth = std::max_element(valueDescriptions.begin(), valueDescriptions.end(),
                     [](const std::string& a, const std::string& b) {
                         return a.size() < b.size();
                     })->size(); //width of the longest string +4
    int drawValueXPos = 0;
    int drawValueYPos = 0; //these are determined dynamically, but we need these for drawing the values with partial updates..
    int getElementWidth() const override { return (elementName.length() + 3 + maxStringWidth + 2)*GLYPH_WIDTH/2; }
    ActionCallback action;
    private:
    
};

class HorizontalActionElement : public UIHorizontalElement {

public:
    using ActionCallback = std::function<void()>;

    HorizontalActionElement(Renderer* renderer,
                  std::string name,
                  ActionCallback callback,
                  std::string extraDesc = "")
        : UIHorizontalElement(renderer,
                    std::move(name)),
          action(std::move(callback))
    {}

    UIElementType getType() const override {
        return UIElementType::HorizontalAction;
    }


    void trigger() {
        ESP_LOGI("UIElements", "Action called");
        if (action) {
            action();
        }
    }

private:
    ActionCallback action;
};