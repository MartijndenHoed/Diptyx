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

enum class UIElementType {
    Menu,
    Value,
    Book,
    Author,
    Action,
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
    void renderElement();
    virtual void renderIcon(int y,bool Highlight);

    std::string elementName;
    std::string elementDescription;
    std::string elementExtraDescription;

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
    void enterElement();
    UIElementType getType() const override { return UIElementType::Menu; }

    std::vector<std::shared_ptr<UIElement>> children;
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
        : UIElement(renderer,
                    book->title,
                    std::to_string(book->currentPage+1) + "/" +
                    (book->totalPageCount > 0 ? std::to_string(book->totalPageCount) : "?"), (book->favorite?std::string("❤"):std::string(""))),
        book(book) {}

    Book *book;
    UIElementType getType() const override { return UIElementType::Book; }
    void renderIcon(int y,bool Highlight) override;
    //std::string elementName = book.title;
    //std::string elementDescription = book.author;
    //std::string elementDescription = std::to_string(book.readPageCount) + "/" + std::to_string(book.totalPageCount);

    private:

};

class AuthorElement : public MenuElement {
public:
    AuthorElement(Renderer* renderer, Author* author)
        : MenuElement(renderer,
                      author->name,
                      "Books: " + std::to_string(author->bookList.size())),
          author(author)
    {}

    void initChildren() {
        children.clear();
        for (Book* book : author->bookList) {
            auto bookElement = std::make_shared<BookElement>(renderer, book);
            addChild(bookElement);
        }
    }

    UIElementType getType() const override { return UIElementType::Author; }
    Author* author;
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