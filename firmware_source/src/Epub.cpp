#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_log.h>
#include <map>
#include "tinyxml2.h"
#include "ZipFile.h"
#include "Epub.h"
#include "device.h"

static const char *TAG = "EPUB";

static std::string decodePercent20(const char* s) {
    if (!s) return std::string();
    std::string out = s;
    size_t pos = 0;
    while ((pos = out.find("%20", pos)) != std::string::npos) {
        out.replace(pos, 3, " ");
        pos += 1; // continue after the inserted space
    }
    return out;
}

bool EpubParser::find_content_opf_file(ZipFile &zip, std::string &content_opf_file)
{
  // open up the meta data to find where the content.opf file lives
  char *meta_info = (char *)zip.read_file_to_memory("META-INF/container.xml");
  if (!meta_info)
  {
    ESP_LOGE(TAG, "Could not find META-INF/container.xml");
    return false;
    
  }
  // parse the meta data
  tinyxml2::XMLDocument meta_data_doc;
  auto result = meta_data_doc.Parse(meta_info);
  // finished with the data as it's been parsed
  free(meta_info);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Could not parse META-INF/container.xml");
    return false;
  }
  auto container = meta_data_doc.FirstChildElement("container");
  if (!container)
  {
    ESP_LOGE(TAG, "Could not find container element in META-INF/container.xml");
    return false;
  }
  auto rootfiles = container->FirstChildElement("rootfiles");
  if (!rootfiles)
  {
    ESP_LOGE(TAG, "Could not find rootfiles element in META-INF/container.xml");
    return false;
  }
  // find the root file that has the media-type="application/oebps-package+xml"
  auto rootfile = rootfiles->FirstChildElement("rootfile");
  while (rootfile)
  {
    const char *media_type = rootfile->Attribute("media-type");
    if (media_type && strcmp(media_type, "application/oebps-package+xml") == 0)
    {
      const char *full_path = rootfile->Attribute("full-path");
      if (full_path)
      {
        content_opf_file = full_path;
        return true;
      }
    }
    rootfile = rootfile->NextSiblingElement("rootfile");
  }
  ESP_LOGE(TAG, "Could not get path to content.opf file");
  return false;
}

std::string normalise_path(const std::string &path)
{
  std::vector<std::string> components;
  std::string component;
  for (auto c : path)
  {
    if (c == '/')
    {
      if (!component.empty())
      {
        if (component == "..")
        {
          if (!components.empty())
          {
            components.pop_back();
          }
        }
        else
        {
          components.push_back(component);
        }
        component.clear();
      }
    }
    else
    {
      component += c;
    }
  }
  if (!component.empty())
  {
    components.push_back(component);
  }
  std::string result;
  for (auto &component : components)
  {
    if (result.size() > 0)
    {
      result += "/";
    }
    result += component;
  }
  return result;
}

bool EpubParser::parse_content_opf(ZipFile &zip, std::string &content_opf_file)
{
  // read in the content.opf file and parse it
  char *contents = (char *)zip.read_file_to_memory(content_opf_file.c_str());
  // parse the contents
  tinyxml2::XMLDocument doc;
  auto result = doc.Parse(contents);
  free(contents);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Error parsing content.opf - %s", doc.ErrorIDToName(result));
    return false;
  }
  auto package = doc.FirstChildElementIgnoreNamespace("package");
  if (!package)
  {
    ESP_LOGE(TAG, "Could not find package element in content.opf");
    return false;
  }
  // get the metadata - title and cover image
  auto metadata = package->FirstChildElementIgnoreNamespace("metadata");
  if (!metadata)
  {
    ESP_LOGE(TAG, "Missing metadata");
    return false;
  }
  auto title = metadata->FirstChildElement("dc:title");
  if (!title)
  {
    ESP_LOGE(TAG, "Missing title");
    m_title = std::string(m_path);
  }
  else m_title = title->GetText();
  
  auto author = metadata->FirstChildElement("dc:creator");

  if (!author)
  {
      ESP_LOGE(TAG, "Missing author");
      m_author = std::string("unknown author");
  }
  else
  {
      auto creator = author;
      bool authorFound = false;

      for (auto current = author; current && !authorFound; current = current->NextSiblingElement("dc:creator"))
      {
          const char* id = current->Attribute("id");
          if (!id) continue;

          for (auto meta = metadata->FirstChildElement("meta"); meta; meta = meta->NextSiblingElement("meta"))
          {
              const char* refines = meta->Attribute("refines");
              const char* property = meta->Attribute("property");
              const char* role = meta->GetText();

              if (refines && property && role &&
                  std::string(refines) == std::string("#") + id &&
                  std::string(property) == "role" &&
                  std::string(role) == "aut")
              {
                  creator = current;
                  authorFound = true;
                  break;
              }
          }
      }

      m_author = creator->GetText() ? creator->GetText() : "unknown author";
  }


  auto cover = metadata->FirstChildElementIgnoreNamespace("meta");
while (cover)
{
    const char* nameAttr = cover->Attribute("name");
    
    // If this tag has a 'name' attribute AND it is 'cover', we found it!
    if (nameAttr && strcmp(nameAttr, "cover") == 0) {
        break; 
    }
    
    // Otherwise, keep looking at the next sibling
    cover = cover->NextSiblingElementIgnoreNamespace("meta");
}
  if (!cover)
  {
    ESP_LOGI(TAG, "Missing cover");
  }
  auto cover_item = cover ? cover->Attribute("content") : nullptr;
  // read the manifest and spine
  // the manifest gives us the names of the files
  // the spine gives us the order of the files
  // we can then read the files in the order they are in the spine
  auto manifest = package->FirstChildElementIgnoreNamespace("manifest");
  if (!manifest)
  {
    ESP_LOGE(TAG, "Missing manifest");
    return false;
  }
  // create a mapping from id to file name
  auto item = manifest->FirstChildElementIgnoreNamespace("item");
  std::map<std::string, std::string> items;
  std::string tocID;

  while (item)
  {
    std::string item_id = item->Attribute("id");
    std::string hrefRaw = decodePercent20(item ? item->Attribute("href") : nullptr);
    std::string href = m_base_path + hrefRaw;
    // grab the cover image
    if (cover_item && item_id == cover_item)
    {
      m_cover_image_item = href;
    }
    // grab the ncx file
    // if (item_id == "ncx"  || item_id == "ncx_toc" || item_id == "toc")
    // {
    //   m_toc_ncx_item = href;
    // }
    items[item_id] = href;

    const char* value = item->Attribute("properties");

    if (value && strcmp(value,"nav")==0)
    {
        m_nav_item = href;
    }

    item = item->NextSiblingElementIgnoreNamespace("item");
  }
  // find the spine
  auto spine = package->FirstChildElementIgnoreNamespace("spine");
  if (!spine)
  {
    ESP_LOGE(TAG, "Missing spine");
    return false;
  }

  const char* tocAttr = spine->Attribute("toc");

  if (tocAttr)
  {
      auto it = items.find(tocAttr);

      if (it != items.end())
      {
          m_toc_ncx_item = it->second;
      }
  }
  else
  {
    ESP_LOGE(TAG, "Missing toc attribute");
  }
  
  // read the spine
  auto itemref = spine->FirstChildElementIgnoreNamespace("itemref");
  while (itemref)
  {
    auto id = itemref->Attribute("idref");
    if (items.find(id) != items.end())
    {
      m_spine.push_back(std::make_pair(id, items[id]));
    }
    itemref = itemref->NextSiblingElementIgnoreNamespace("itemref");
  }
  
  return true;
}

bool EpubParser::parse_toc_ncx_file(ZipFile &zip)
{
    // The ncx file should have been specified in the content.opf file
    if (m_toc_ncx_item.empty())
    {
        ESP_LOGE(TAG, "No ncx file specified");
        return false;
    }

    ESP_LOGI(TAG, "toc path: %s\n", m_toc_ncx_item.c_str());

    char *ncx_data = (char *)zip.read_file_to_memory(m_toc_ncx_item.c_str());
    if (!ncx_data)
    {
        ESP_LOGE(TAG, "Could not find %s", m_toc_ncx_item.c_str());
        return false;
    }

    // Parse the TOC contents
    tinyxml2::XMLDocument doc;
    auto result = doc.Parse(ncx_data);
    free(ncx_data);

    if (result != tinyxml2::XML_SUCCESS)
    {
        ESP_LOGE(TAG, "Error parsing toc %s", doc.ErrorIDToName(result));
        return false;
    }

    auto ncx = doc.FirstChildElement("ncx");
    if (!ncx)
    {
        ESP_LOGE(TAG, "Could not find first child ncx in toc");
        return false;
    }

    auto navMap = ncx->FirstChildElement("navMap");
    if (!navMap)
    {
        ESP_LOGE(TAG, "Could not find navMap child in ncx");
        return false;
    }

    // Recursive function to process navPoints and their children
    std::function<void(tinyxml2::XMLElement*, int)> processNavPoint;

    processNavPoint = [&](tinyxml2::XMLElement *navPoint, int depth)
    {
        while (navPoint)
        {
            auto navLabelElement = navPoint->FirstChildElement("navLabel");
            auto textElement = navLabelElement
                ? navLabelElement->FirstChildElement("text")
                : nullptr;

            auto navLabel = textElement ? textElement->FirstChild() : nullptr;

            std::string title = "";
            if (navLabel && navLabel->Value())
            {
                title = navLabel->Value();
            }

            // Indent subchapters with a leading space
            if (depth > 0)
            {
                title.insert(0, "·");
            }

            auto content = navPoint->FirstChildElement("content");
            std::string src = decodePercent20(
                content ? content->Attribute("src") : nullptr
            );

            std::string href = m_base_path + src;

            // Split the href on # to get the href and anchor
            size_t pos = href.find('#');
            std::string anchor = "";

            if (pos != std::string::npos)
            {
                anchor = href.substr(pos + 1);
                href = href.substr(0, pos);
            }

            m_toc.push_back(EpubTocEntry(title, href, anchor, 0));

            ESP_LOGI(TAG, "%s -> %s#%s",
                     title.c_str(), href.c_str(), anchor.c_str());

            // Process nested navPoints (subchapters)
            auto childNavPoint = navPoint->FirstChildElement("navPoint");
            if (childNavPoint)
            {
                processNavPoint(childNavPoint, depth + 1);
            }

            navPoint = navPoint->NextSiblingElement("navPoint");
        }
    };

    // Process top-level chapters
    auto navPoint = navMap->FirstChildElement("navPoint");
    processNavPoint(navPoint, 0);

    return true;
}

bool EpubParser::parse_nav_file(ZipFile &zip)
{
    // The nav file should have been specified in the content.opf file
    if (m_nav_item.empty())
    {
        ESP_LOGE(TAG, "No nav file specified");
        return false;
    }

    ESP_LOGI(TAG, "nav path: %s\n", m_nav_item.c_str());

    char *nav_data = (char *)zip.read_file_to_memory(m_nav_item.c_str());
    if (!nav_data)
    {
        ESP_LOGE(TAG, "Could not find %s", m_nav_item.c_str());
        return false;
    }

    // Parse the TOC contents
    tinyxml2::XMLDocument doc;
    auto result = doc.Parse(nav_data);
    free(nav_data);

    if (result != tinyxml2::XML_SUCCESS)
    {
        ESP_LOGE(TAG, "Error parsing toc %s", doc.ErrorIDToName(result));
        return false;
    }

    auto html = doc.FirstChildElement("html");
    if (!html)
    {
        ESP_LOGE(TAG, "Could not find first child html in toc");
        return false;
    }

    auto body = html->FirstChildElement("body");
    if (!body)
    {
        ESP_LOGE(TAG, "Could not find body child in html");
        return false;
    }

    auto nav = body->FirstChildElement("nav");

    // Get the directory containing the nav document
    std::string navBasePath =
        m_nav_item.substr(0, m_nav_item.find_last_of('/') + 1);

    // Recursive function to process li elements and their nested ol elements
    std::function<void(tinyxml2::XMLElement*, int)> processListItems;

    processListItems = [&](tinyxml2::XMLElement *li, int depth)
    {
        while (li)
        {
            auto a = li->FirstChildElement("a");

            if (a)
            {
                const char* text = a->GetText();
                std::string title = text ? text : "";

                // Indent subchapters with a leading space
                if (depth > 0)
                {
                    title.insert(0, "·");
                }

                const char* hrefAttr = a->Attribute("href");

                if (!hrefAttr)
                {
                    ESP_LOGE(TAG, "Missing href in nav a element");
                }
                else
                {
                    std::string src = decodePercent20(hrefAttr);

                    // Split the href on # to get the path and anchor
                    size_t pos = src.find('#');
                    std::string anchor = "";

                    if (pos != std::string::npos)
                    {
                        anchor = src.substr(pos + 1);
                        src = src.substr(0, pos);
                    }

                    std::string href = normalise_path(navBasePath + src);

                    m_toc.push_back(EpubTocEntry(title, href, anchor, 0));

                    ESP_LOGI(TAG, "%s -> %s#%s",
                             title.c_str(), href.c_str(), anchor.c_str());
                }
            }
            else
            {
                ESP_LOGE(TAG, "Missing a element in nav li");
            }

            // Process nested subchapters
            auto ol = li->FirstChildElement("ol");

            if (ol)
            {
                auto childLi = ol->FirstChildElement("li");

                if (childLi)
                {
                    processListItems(childLi, depth + 1);
                }
            }

            li = li->NextSiblingElement("li");
        }
    };

    // Find the TOC navigation element
    while (nav)
    {
        const char* typeAttr = nav->Attribute("epub:type");

        if (typeAttr && strcmp(typeAttr, "toc") == 0)
        {
            auto ol = nav->FirstChildElement("ol");

            if (!ol)
            {
                ESP_LOGE(TAG, "Could not find ol child in nav");
                return false;
            }

            auto li = ol->FirstChildElement("li");

            if (!li)
            {
                ESP_LOGE(TAG, "Could not find li child in ol");
            }
            else
            {
                processListItems(li, 0);
            }
        }

        nav = nav->NextSiblingElement("nav");
    }

    return true;
}

EpubParser::EpubParser(const std::string &path)
    : FileParser(path),
      m_path(path)
{
}

// load in the meta data for the epub file
bool EpubParser::load(Book *activeBook)
{
  ESP_LOGE(TAG, "loading EPUB: %s", m_path.c_str());
  this->activeBook = activeBook;
  ZipFile zip(m_path.c_str());
  ESP_LOGE(TAG, "loading EPUB: %s", m_path.c_str());
  std::string content_opf_file;
  if (!find_content_opf_file(zip, content_opf_file))
  {
    return false;
  }
  // get the base path for the content
  m_base_path = content_opf_file.substr(0, content_opf_file.find_last_of('/') + 1);
  if (!parse_content_opf(zip, content_opf_file))
  {
    return false;
  }
  if(!m_toc_ncx_item.empty())
  {
    if (!parse_toc_ncx_file(zip))
    {
      return false;
    }
  }
  else if(!m_nav_item.empty())
  {
    if (!parse_nav_file(zip))
    {
      return false;
    }
  }
  else 
  {
    ESP_LOGE(TAG, "No ncx or nav file specified");
    return false;
  }
  activeBook->author = get_author();
  activeBook->title = get_title();
  activeBook->path = get_path();


  return true;
}

int EpubParser::getCurrentPageInChapter(int currentPage)
{
  int pageCounter = currentPage;
  for(int i = 0;i<this->get_spine_items_count();i++)
  {
        if(pageCounter<activeBook->chapterPageCounts[i]) return pageCounter;
        pageCounter-= activeBook->chapterPageCounts[i];
  }
  return 0;
}

std::string EpubParser::getPath() {
  return get_path();
}

std::string EpubParser::getCoverImagePath() {
  return get_cover_image_item();
}

int EpubParser::getCurrentChapter(int currentPage)
{
    int pageCounter = currentPage;
    for(int i = 0;i<this->get_spine_items_count();i++)
    {
        pageCounter-= activeBook->chapterPageCounts[i];
        if(pageCounter<0) return i;
    }
    return 0;
}

bool EpubParser::renderPageToBuffers(int pageNumber, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer, int pageCacheIndex)
{
  int currentPageInChapter = getCurrentPageInChapter(pageNumber);
  int currentChapter = getCurrentChapter(pageNumber);
  std::string currentChapterPath = get_spine_item(currentChapter);
  char *html = reinterpret_cast<char *>(get_item_contents(currentChapterPath));
  if(html)
    {
        Device::getInstance().renderer->clearScreenBuffer(leftScreenBuffer);
        Device::getInstance().renderer->clearScreenBuffer(rightScreenBuffer);
        HtmlParser *parser = nullptr;
        parser = new HtmlParser(html, strlen(html), currentChapterPath,currentPageInChapter,this,leftScreenBuffer,rightScreenBuffer);
        parser->cachedImages = activeBook->cachedImages;
        parser->parse();
        pageElementIndex[pageCacheIndex] = parser->currentElementIndex;
        pageChapterIndex[pageCacheIndex] = currentChapter;
        pageImagePresent[pageCacheIndex] = parser->imagePresentOnPage;
        free(html);
        delete parser;
        return true;
    }

  return false;
}

bool EpubParser::indexChapters()
{
    activeBook->wordCount = 0;
    activeBook->chapterPageCounts.clear();
    activeBook->cachedImages.clear();
    activeBook->totalPageCount = 0;
    activeBook->chapterCount = get_spine_items_count();
    float percent = 0;
    float prevPercent = 0;
    ESP_LOGI(TAG, "indexing %d chapters", activeBook->chapterCount);
    for(int i=0;i<get_spine_items_count();i++)
    {
        //ESP_LOGI(TAG, "indexing chapter %d out of %d", i,get_spine_items_count());
        percent += 1/float(get_spine_items_count());
        if(percent>=prevPercent+0.1) 
        {
            prevPercent += 0.1;
            if(!(Device::getInstance().state == Device::State::simpleReader)) Device::getInstance().notificationHandler->drawIndexingNotification(activeBook->title,int(100*percent));
        }
        std::string currentChapterPath = get_spine_item(i);
        char *html = reinterpret_cast<char *>(get_item_contents(currentChapterPath));
        if(html)
        {
            HtmlParser *parser = nullptr;
            ESP_LOGI(TAG, "render Chapter: %s", currentChapterPath.c_str());
            parser = new HtmlParser(html, strlen(html), currentChapterPath,-2,this,nullptr,nullptr);
            parser->indexingMode = true;
            parser->currentChapterIndex = i;
            parser->currentChapterStartPageIndex = activeBook->totalPageCount;
            parser->bookMarks = activeBook->bookMarks;
            parser->cachedImages = activeBook->cachedImages;
            Book::BookMark currentPageData = Book::BookMark(activeBook->currentPage,activeBook->currentPageChapterIndex,activeBook->currentPageElementIndex);
            parser->bookMarks.push_back(currentPageData);
            ESP_LOGI(TAG, "requested element: %d", activeBook->currentPageElementIndex);
            parser->parse();
            if(activeBook->currentPageChapterIndex==i) 
            {
                activeBook->currentPage = parser->bookMarks[parser->bookMarks.size()-1].pageIndex;
                if(activeBook->currentPage%2) activeBook->currentPage-=1;
            }
            parser->bookMarks.pop_back();
            activeBook->bookMarks = parser->bookMarks;
            activeBook->cachedImages = parser->cachedImages;
            int pageCount = (parser->pageReturn+1);
            if(pageCount%2) pageCount+=1;
            activeBook->chapterPageCounts.push_back(pageCount);
            activeBook->totalPageCount += pageCount;
            activeBook->wordCount += parser->contentParser->wordCount;
            free(html);
            delete parser;
        } 
    }
    return true;
}

int EpubParser::getTOCLength()
{
  return get_toc_items_count();
}

int EpubParser::getChapterIndexFromTOCindex(int TOCitemIndex)
{
  return get_spine_index_for_toc_index(TOCitemIndex);
}

int EpubParser::getTocIndexFromChapterindex(int chapterIndex)
{
  return get_toc_index_for_spine_index(chapterIndex);
}


std::string EpubParser::getTOCItemTitle(int TOCitemIndex)
{
  return get_toc_item(TOCitemIndex).title;
}

const std::string &EpubParser::get_title()
{
  return m_title;
}

const std::string &EpubParser::get_author()
{
  return m_author;
}

const std::string &EpubParser::get_cover_image_item()
{
  return m_cover_image_item;
}


uint8_t *EpubParser::get_item_contents(const std::string &item_href, size_t *size)
{
  ZipFile zip(m_path.c_str());
  std::string path = normalise_path(item_href);
  auto content = zip.read_file_to_memory(path.c_str(), size);
  if (!content)
  {
    ESP_LOGE(TAG, "Failed to read item %s", path.c_str());
    return nullptr;
  }
  return content;
}

int EpubParser::get_spine_items_count()
{
  return m_spine.size();
}

std::string &EpubParser::get_spine_item(int spine_index)
{
  if (spine_index < 0 || spine_index >= static_cast<int>(m_spine.size())) {
      ESP_LOGI(TAG, "get_spine_item index:%d is out_of_range", spine_index);
      spine_index = 0;
  }
  return m_spine[spine_index].second;
}

EpubTocEntry &EpubParser::get_toc_item(int toc_index)
{
  return m_toc[toc_index];
}

int EpubParser::get_toc_items_count()
{
  return m_toc.size();
}

// work out the section index for a toc index
int EpubParser::get_spine_index_for_toc_index(int toc_index)
{
  // the toc entry should have an href that matches the spine item
  // so we can find the spine index by looking for the href
  for (int i = 0; i < m_spine.size(); i++)
  {
    if (m_spine[i].second == m_toc[toc_index].href)
    {
      return i;
    }
  }
  ESP_LOGI(TAG, "Section not found");
  // not found - default to the start of the book
  return 0;
}

int EpubParser::get_toc_index_for_spine_index(int spine_index)
{
  // the toc entry should have an href that matches the spine item
  // so we can find the spine index by looking for the href
  for (int i = 0; i < m_toc.size(); i++)
  {
    if (m_toc[i].href == m_spine[spine_index].second)
    {
      return i;
    }
  }
  ESP_LOGI(TAG, "Section not found");
  // not found - default to the start of the book
  return -1;
}


