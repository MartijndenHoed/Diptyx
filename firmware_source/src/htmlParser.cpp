#include <htmlParser.h>
#include <esp_log.h>

//This code is a heavily modified version of Atomic 14's Epub reader
static const char *TAG = "HTML Parser";

const std::vector<std::string_view> HEADER_TAGS = {"h1", "h2", "h3", "h4", "h5", "h6"};
const std::vector<std::string_view> BLOCK_TAGS  = {"p", "li", "div", "blockquote", "svg","table","tbody","tr","td"};
const std::vector<std::string_view> EMPTY_LINE_TAGS = {"br", "br/"};
const std::vector<std::string_view> INLINE_TAGS = {"span", "a"};
const std::vector<std::string_view> BOLD_TAGS = {"b"};
const std::vector<std::string_view> ITALIC_TAGS = {"i", "em"};
const std::vector<std::string_view> IMAGE_TAGS = {"img"};
const std::vector<std::string_view> COVER_TAGS = {"image"};
const std::vector<std::string_view> SKIP_TAGS = {"title", "style"};
const std::vector<std::string_view> BODY_TAGS = {"body"};
const std::vector<std::string_view> LINK_TAGS = {"link"};

inline bool matches(std::string_view tag_name,
                    const std::vector<std::string_view>& possible_tags)
{
    return std::find(possible_tags.begin(), possible_tags.end(), tag_name)
           != possible_tags.end();
}

static std::string normalize_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (item.empty() || item == ".") continue;
        if (item == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(item);
        }
    }

    // Reconstruct normalized path
    std::string normalized;
    for (size_t i = 0; i < parts.size(); i++) {
        normalized += parts[i];
        if (i + 1 < parts.size()) normalized += "/";
    }
    return normalized;
}

// Resolve relative or absolute EPUB path
std::string resolve_relative_path(const std::string& base_file,
                                  const std::string& relative,
                                  const std::string& epub_base) {
    std::string full_path;

    if (relative.empty()) {
        return "";
    }

    if (relative[0] == '/') {
        // Absolute path inside EPUB → relative to epub_base
        full_path = epub_base + relative.substr(1); // strip leading '/'
    } else {
        // Relative path → relative to base_file’s directory
        std::string base_dir = base_file.substr(0, base_file.find_last_of("/\\") + 1);
        full_path = base_dir + relative;
    }

    return normalize_path(full_path);
}

HtmlParser::HtmlParser(const char *html, int length, const std::string &base_path,Renderer* renderer,int page, Epub *epub, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer)
{
  this->base_path = base_path;
  this->targetPage = page;
  this->currentPage = 0;
  this->renderer = renderer;
  this->contentParser = new ContentParser(renderer);
  this->epub = epub;
  this->leftScreenBuffer = leftScreenBuffer;
  this->rightScreenBuffer = rightScreenBuffer;
  if(this->renderer) this->renderer->framebuffer = leftScreenBuffer;
  this->htmlLength = length;
  this->htmlRaw = html;
}

HtmlParser::~HtmlParser()
{
  delete contentParser;
}

bool HtmlParser::VisitEnter(const tinyxml2::XMLElement &element, const tinyxml2::XMLAttribute *firstAttribute)
{
  if(doneParsing) return false;
  const char *tag_name = element.Name();

  //Walk through the list of tags and see what matches
  if (matches(tag_name, IMAGE_TAGS))
  {
    const char *src = element.Attribute("src");
    if (src)
    {
      std::string srcStr = std::string(src);
      if ( (srcStr.size() >= 4 && srcStr.compare(srcStr.size() - 4, 4, ".png") == 0) || (srcStr.size() >= 4 && srcStr.compare(srcStr.size() - 4, 4, ".jpg") == 0) || (srcStr.size() >= 5 && srcStr.compare(srcStr.size() - 5, 5, ".jpeg") == 0)){
        //src is an image
       
        ESP_LOGI(TAG, "image found found: %s", src);

        std::string imagePath = resolve_relative_path(
        this->base_path,                 // full path of current HTML
        srcStr,             // src
        this->epub->get_base_path()      // EPUB root path
        );

        ESP_LOGI(TAG, "loading image: %s", src);
        Image image = Image(imagePath,epub->get_path());
        bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
        if(!drawPage && indexingMode == false)
        {
          for(int i=0;i<cachedImages.size();i++)
          {
            if(cachedImages[i].filePath==imagePath)
            {
              ESP_LOGI(TAG, "loaded image dimensions from cache");
              image.cached=true;
              image.imageHeight = cachedImages[i].height;
              image.imageWidth = cachedImages[i].width;
              i=cachedImages.size();
            }
          }
        }

        this->styleHierarchy.emplace_back(this->styleHierarchy.back());
        const char *styleClass = element.Attribute("class");
        if(styleClass)
        {
            this->styleHierarchy.back().applyClass(cssCache, styleClass);
        }

        contentParser->parseImage(image,styleHierarchy.back(),drawPage);
        ESP_LOGI(TAG, "done drawing image: %s", src);

        if(drawPage && !contentParser->imageOverflowBuffer) imagePresentOnPage=true;

        if(contentParser->imageOverflowBuffer) //If the image overflows to the next page, we don't draw it yet, but push it to the next page
        {
            contentParser->finishPage();
            this->currentPage++;
            if(this->currentPage==this->targetPage+1) renderer->framebuffer=rightScreenBuffer;
            if(this->currentPage>this->targetPage+1 && !this->indexingMode) doneParsing = true;
            bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
            if(drawPage) imagePresentOnPage=true;
            contentParser->parseOverflowedImage(styleHierarchy.back(), drawPage);
        }

        if(image.cached==false) cachedImages.emplace_back(imagePath,image.imageHeight,image.imageWidth);
        this->styleHierarchy.pop_back();
      }

    }
    else
    {
      ESP_LOGI(TAG, "Could not find src attribute");
    }
  }
  else if (matches(tag_name, COVER_TAGS))
  {
    const char *src = element.Attribute("xlink:href");
    if (src)
    {
      std::string srcStr = std::string(src);

      if ( (srcStr.size() >= 4 && srcStr.compare(srcStr.size() - 4, 4, ".jpg") == 0) || (srcStr.size() >= 5 && srcStr.compare(srcStr.size() - 5, 5, ".jpeg") == 0)) {
      // src ends with ".jpg"
        ESP_LOGI(TAG, "image found found: %s", src);
        std::string imagePath = resolve_relative_path(
        this->base_path,                 // full path of current HTML
        srcStr,             // src
        this->epub->get_base_path()      // EPUB root path
        );
        Image image = Image(imagePath,epub->get_path());
        bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
        if(drawPage) imagePresentOnPage=true;

        this->styleHierarchy.emplace_back(this->styleHierarchy.back());
        const char *styleClass = element.Attribute("class");
        if(styleClass)
        {
            this->styleHierarchy.back().applyClass(cssCache, styleClass);
        }

        this->styleHierarchy.back().width = EPD_HEIGHT; //'image' tags are only used on covers, and should cover the full page
        this->styleHierarchy.back().height = EPD_WIDTH;
        this->styleHierarchy.back().widthSet = true;
        this->styleHierarchy.back().heightSet = true;

        contentParser->parseImage(image,styleHierarchy.back(),drawPage);
        this->styleHierarchy.pop_back();
      }

    }
    else
    {
      ESP_LOGI(TAG, "Could not find src attribute");
    }
  }
  else if (matches(tag_name, SKIP_TAGS))
  {
    return false;
  }
  else if (matches(tag_name, BODY_TAGS))
  {
    this->styleHierarchy.emplace_back();
    this->styleHierarchy.back().height = EPD_WIDTH;
    this->styleHierarchy.back().width = EPD_HEIGHT;
  }
  else if (matches(tag_name, HEADER_TAGS))
  {
    this->styleHierarchy.emplace_back(this->styleHierarchy.back());
    this->styleHierarchy.back().fontSize = 2;
    this->isNewLine = true;
    const char *styleClass = element.Attribute("class");
    if(styleClass)
    {
        this->styleHierarchy.back().applyClass(cssCache, styleClass);
    }
  }
  else if (matches(tag_name, EMPTY_LINE_TAGS))
  {
    bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
    this->contentParser->finishLine(drawPage,styleHierarchy.back().align);
    isNewLine = true;
  }
  else if (matches(tag_name, BLOCK_TAGS))
  {
    this->isNewLine = true;

    this->styleHierarchy.emplace_back(this->styleHierarchy.back());

    const char *styleClass = element.Attribute("class");
    if(styleClass)
    {
        this->styleHierarchy.back().applyClass(cssCache, styleClass);
    }
  }
  else if (matches(tag_name, INLINE_TAGS))
  {
    precedingWhiteSpace = element.hasPrecedingWhitespace;
    this->styleHierarchy.emplace_back(this->styleHierarchy.back());

    const char *styleClass = element.Attribute("class");
    if(styleClass)
    {
      this->styleHierarchy.back().applyClass(cssCache, styleClass);
    }
  }
  else if (matches(tag_name, BOLD_TAGS))
  {
    this->styleHierarchy.emplace_back(this->styleHierarchy.back());
    this->styleHierarchy.back().bold = true;
        const char *styleClass = element.Attribute("class");
    if(styleClass)
    {
        this->styleHierarchy.back().applyClass(cssCache, styleClass);
    }
  }
  else if (matches(tag_name, ITALIC_TAGS))
  {
    this->styleHierarchy.emplace_back(this->styleHierarchy.back());
    this->styleHierarchy.back().italic = true;
        const char *styleClass = element.Attribute("class");
    if(styleClass)
    {
        this->styleHierarchy.back().applyClass(cssCache, styleClass);
    }
  }
    else if (matches(tag_name, LINK_TAGS))
  {
    const char *rel = element.Attribute("rel");
    ESP_LOGI(TAG, "link found in chapter: %s",rel);
    if (rel)
    {
        if(strcmp(rel,"stylesheet")==0)
        {
          const char *href = element.Attribute("href");
          if(href)
          {
            std::string hrefStr(href);
            if (hrefStr.size() >= 4 && hrefStr.compare(hrefStr.size() - 4, 4, ".css") == 0) {
                // rel ends with ".css"
              ESP_LOGI(TAG, "css stylesheet found in chapter: %s", href);
              this->styleSheetRef = hrefStr;

              std::string css_path = resolve_relative_path(
                  this->base_path,                 // full path of current HTML
                  this->styleSheetRef,             // href
                  this->epub->get_base_path()      // EPUB root path
              );
              ESP_LOGI(TAG, "css path: %s", css_path.c_str());
              size_t css_size;
              uint8_t* raw_css = this->epub->get_item_contents(css_path, &css_size);
              if (raw_css) {
                  std::string css(reinterpret_cast<char *>(raw_css), css_size); // construct with length
                  free(raw_css);
                  merge_css(this->cssCache, parse_css_string(css));
              }
              
              //ESP_LOGI(TAG, "css style for h1: %s",cssCache["h1"]["font-size"].c_str());
            }

          }
          
        }
    }
  }
  return true;
}
/// Visit a text node.
bool HtmlParser::Visit(const tinyxml2::XMLText &text)
{
  if(doneParsing) return false;
  std::string textValue = text.Value();
  if(this->precedingWhiteSpace)
  {
    this->precedingWhiteSpace = false;
    if(!this->isNewLine) textValue.insert(textValue.begin(), ' ');
  }

  if(this->indexingMode)
  {
    for(Book::BookMark &bookMark : this->bookMarks)
    {
      if(bookMark.chapterIndex==this->currentChapterIndex && bookMark.elementIndex==this->currentElementIndex) 
      {
        bookMark.pageIndex=currentPage + currentChapterStartPageIndex;
        if(bookMark.pageIndex%2) bookMark.pageIndex-=1;
      }

    }

  }
  if(this->currentPage<this->targetPage || this->indexingMode) currentElementIndex++;


  std::vector<int> intArray = decodeHtmlEntities(utf8ToCodePoints(textValue));
  
  bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
  this->contentParser->parseTextBlock(intArray,this->isNewLine,styleHierarchy.back(),drawPage);
  
  this->isNewLine = false;
  if(contentParser->textOverflowBuffer.size()) 
  {
    this->currentPage++;
    if(this->currentPage==this->targetPage+1) renderer->framebuffer=rightScreenBuffer;
    if(this->currentPage>this->targetPage+1 && !this->indexingMode) doneParsing = true;
    this->emptyTextOverflow();
  }
  if(doneParsing) return false;
  return true;
}

void HtmlParser::emptyTextOverflow()
{
  
  bool textOverFlowRemaining = true;
  while(textOverFlowRemaining)
  {
    bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
    std::vector <int> textOverflowBufferCopy = contentParser->textOverflowBuffer;
    contentParser->textOverflowBuffer.clear();
    this->contentParser->parseTextBlock(textOverflowBufferCopy,false,styleHierarchy.back(),drawPage);
    if(contentParser->textOverflowBuffer.size())
    {
      this->currentPage++;
      if(this->currentPage==this->targetPage+1) renderer->framebuffer=rightScreenBuffer;
      if(this->currentPage>this->targetPage+1 && !this->indexingMode) doneParsing = true;
    }
    else
    {
      textOverFlowRemaining = false;
    }
  }
}


bool HtmlParser::VisitExit(const tinyxml2::XMLElement &element)
{
  if(doneParsing) return false;
  const char *tag_name = element.Name();
  // ESP_LOGI(TAG, "element exited: %s", element.Name());
  // ESP_LOGI(TAG, "style array length: %d", styleHierarchy.size());
  if (matches(tag_name, HEADER_TAGS))
  {
    bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
    this->contentParser->finishLine(drawPage,styleHierarchy.back().align);
    styleHierarchy.pop_back();
  }
  else if (matches(tag_name, BLOCK_TAGS))
  {
    bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
    this->contentParser->finishLine(drawPage,styleHierarchy.back().align);
    styleHierarchy.pop_back();
  }
    else if (matches(tag_name, INLINE_TAGS))
  {
    styleHierarchy.pop_back();
  }
  else if (matches(tag_name, BOLD_TAGS))
  {
    styleHierarchy.pop_back();
  }
  else if (matches(tag_name, ITALIC_TAGS))
  {
    styleHierarchy.pop_back();
  }
  else if(matches(tag_name, BODY_TAGS)) //end of chapter
  {
    bool drawPage = (this->currentPage==this->targetPage || this->currentPage==this->targetPage+1);
    this->contentParser->finishLine(drawPage);
    this->contentParser->finishPage(drawPage);

    //in both these cases, no page is rendered:
    if(this->indexingMode) this->pageReturn = this->currentPage; //return the pagecount if we're indexing
    if(this->targetPage > this->currentPage)
    {
      this->pageReturn = -1; //return -1 if a page past this chapter is requested
    }

  }
  //ESP_LOGI(TAG, "style array length: %d", styleHierarchy.size());
  return true;
}

void HtmlParser::parse()
{
  tinyxml2::XMLDocument doc(false, tinyxml2::PEDANTIC_WHITESPACE);
  doc.Parse(htmlRaw, htmlLength);
  doc.Accept(this);
}