//-*-C++-*-
#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <string>
#include <vector>

#include "Defaults.h"

// Structured document class.
//
// Currently only renders to HTML but the intent is to be able to render to
// text/plain too.
class Document {
public:
  // Base class for document node types.
  struct Node {
    virtual void renderHtml(std::ostream &os) const = 0;
  };

  // Intermediate base for leaf node types.
  struct Leaf: public Node {
  };

  // An unadorned string.
  struct String: public Leaf {
    std::string text;
    String() {}
    String(const std::string &s): text(s) {}
    String(int n);

    void renderHtml(std::ostream &os) const;
  };

  // Base class for ordered containers.
  struct LinearContainer: public Node {
    ~LinearContainer();
    std::vector<Node *> nodes;
    Node *append(Node *node) { 
      nodes.push_back(node);
      return node;
    }
    String *append(const std::string &text) {
      String *s = new String(text);
      nodes.push_back(s);
      return s;
    }

    // Render all the contents in order.
    void renderHtmlContents(std::ostream &os) const;
  };

  // A paragraph.
  struct Paragraph: public LinearContainer {
    Paragraph() {}
    Paragraph(Node *node) {
      append(node);
    }
    Paragraph(const std::string &s) {
      append(s);
    }
    void renderHtml(std::ostream &os) const;
  };
  
  // A heading.  Level 1 is the highest-level (biggest) heading, level 6 the
  // lowest (smallest).  Don't use levels <1 or >6.
  struct Heading: public LinearContainer {
    int level;
    Heading(int level_ = 1): level(level_) {}
    Heading(Node *node, int level_ = 1): level(level_) {
      append(node);
    }
    void renderHtml(std::ostream &os) const;
  };

  // A cell in a table.
  //
  // The origin (x=y=0) is at the top left.  Don't add overlapping cells to a
  // single table.
  struct Cell: public LinearContainer {
    Cell(int w_ = 1, int h_ = 1):
      heading(false), x(0), y(0), w(w_), h(h_) {}
    Cell(const std::string &s, int w_ = 1, int h_ = 1):
      heading(false), x(0), y(0), w(w_), h(h_) {
      append(new String(s));
    }
    Cell(Node *n, int w_ = 1, int h_ = 1):
      heading(false), x(0), y(0), w(w_), h(h_) {
      append(n);
    }
    bool heading;                       // true for table-heading cells
    int x, y;                           // position in table
    int w, h;                           // size of cell
    void renderHtml(std::ostream &os) const;
  };
  
  // A table.
  struct Table: public Node {
    Table(): x(0), y(0) {}
    ~Table();

    // TODO we do a lot of O(n^2) passes over this; can we do better?
    std::vector<Cell *> cells;
    int width() const;
    int height() const;
    
    // Add a cell at the cursor position
    void addCell(Cell *cell);
    void addHeadingCell(Cell *cell) {
      cell->heading = true;
      addCell(cell);
    }

    // Start a new row
    void newRow();

    // Add a cell at a particiluar position
    void addCell(Cell *cell, int x, int y);

    // Return the cell that occupies a position or NULL
    Cell *occupied(int x, int y) const;

    void renderHtml(std::ostream &os) const;
    int x, y;                           // current cursor
  };

  // The root container for the document.
  struct RootContainer: public LinearContainer {
    void renderHtml(std::ostream &os) const;
  };

  RootContainer content;                // document contents
  std::string title;                    // document title

  // Append something to the document.
  Node *append(Node *node) { 
    return content.append(node);
  }
  
  // Append a heading to the document.
  Heading *heading(const std::string &text, int level = 1) {
    Heading *h = new Heading(new String(text), level);
    content.append(h);
    return h;
  }

  // Append a paragraph containing some text to the document.
  Paragraph *para(const std::string &text) {
    Paragraph *p = new Paragraph(new String(text));
    content.append(p);
    return p;
  }

  // Render the document as HTML.
  void renderHtml(std::ostream &os) const;

  static void quoteHtml(std::ostream &os, const std::string &s);
  
};

#endif /* DOCUMENT_H */

