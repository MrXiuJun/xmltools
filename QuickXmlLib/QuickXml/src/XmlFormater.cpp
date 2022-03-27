#include <algorithm>
#include "XmlFormater.h"

namespace QuickXml {
	static inline void ltrim(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n');
		}));
	}
	static inline void rtrim(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n');
		}).base(), s.end());
	}
	static inline void trim(std::string& s) {
		ltrim(s);
		rtrim(s);
	}

	static inline void ltrim_s(std::string& s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return (ch != ' ' && ch != '\t');
		}));
	}
	static inline void rtrim_s(std::string& s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return (ch != ' ' && ch != '\t');
		}).base(), s.end());
	}
	static inline void trim_s(std::string& s) {
		ltrim_s(s);
		rtrim_s(s);
	}

	static inline std::string to_lowercase(std::string text) {
		std::transform(text.begin(), text.end(), text.begin(), std::tolower);
		return text;
	}

	static inline bool ends_with(std::string const& text, std::string const& suffix) {
		if (text.length() >= suffix.length()) {
			return (0 == text.compare(text.length() - suffix.length(), suffix.length(), suffix));
		}
		else {
			return false;
		}
	}

	bool XmlFormater::isIdentAttribute(std::string attr) {
		for (std::vector<std::string>::iterator it = this->params.identityAttribues.begin(); it != this->params.identityAttribues.end(); ++it) {
			std::string lw_attr = to_lowercase(attr);
			if (!lw_attr.compare(*it) || ends_with(lw_attr, ":" + *it)) {
				return true;
			}
		}

		return false;
	}

	XmlFormater::XmlFormater(const char* data, size_t length) {
		this->parser = new XmlParser(data, length);

		this->init(data, length, this->getDefaultParams());
	}

	XmlFormater::XmlFormater(const char* data, size_t length, XmlFormaterParamsType params) {
		this->init(data, length, params);
	}

	XmlFormater::~XmlFormater() {
		this->reset();
		if (this->parser != NULL) {
			delete this->parser;
		}
	}

	void XmlFormater::init(const char* data, size_t length) {
		this->init(data, length, this->getDefaultParams());
	}

	void XmlFormater::init(const char* data, size_t length, XmlFormaterParamsType params) {
		if (this->parser != NULL) {
			delete this->parser;
		}

		this->parser = new XmlParser(data, length);
		this->params = params;
		this->reset();
	}

	void XmlFormater::reset() {
		this->indentLevel = 0;
		this->levelCounter = 0;
		this->out.clear();
		this->out.str(std::string());	// make the stringstream empty
	}

	std::string XmlFormater::debugTokens(std::string separator, bool detailed) {
		this->reset();
		this->parser->reset();

		XmlToken token;
		std::stringstream out;

		while ((token = this->parser->parseNext()).type != XmlTokenType::EndOfFile) {
			out << separator << this->parser->getTokenName();

			if (detailed) {
				out << ": ";
				out.write(token.chars, token.size);
			}
		}

		// return result after having removed the firt '/'
		return out.str().erase(0, separator.length());
	}

	std::stringstream* XmlFormater::linearize() {
		this->reset();
		this->parser->reset();

		XmlToken token = { XmlTokenType::Undefined }, nexttoken;
		XmlTokenType lastAppliedTokenType = XmlTokenType::Undefined;
		bool applyAutoclose = false;

		while ((token = this->parser->parseNext()).type != XmlTokenType::EndOfFile) {
			switch (token.type) {
				case XmlTokenType::LineBreak: {
					break;
				}
				case XmlTokenType::Whitespace: {
					if (this->params.applySpacePreserve && this->parser->isSpacePreserve()) {
						lastAppliedTokenType = XmlTokenType::Whitespace;
						this->out.write(token.chars, token.size);
					}
					else if (token.context.inOpeningTag) {
						lastAppliedTokenType = XmlTokenType::Whitespace;
						this->out << " ";
					}
					break;
				}
				case XmlTokenType::Text: {
					if (this->params.applySpacePreserve && this->parser->isSpacePreserve()) {	// whitespace only text nodes must be conserved due to xml:space="preserve"
						lastAppliedTokenType = XmlTokenType::Text;
						this->out.write(token.chars, token.size);
					}
					else {
						std::string tmp(token.chars, token.size);
						trim(tmp);
						if (this->params.ensureConformity) {
							nexttoken = this->parser->getNextToken();
							if (tmp.length() > 0 ||
								((nexttoken.type != XmlTokenType::TagOpening &&
									nexttoken.type != XmlTokenType::Comment &&
									nexttoken.type != XmlTokenType::DeclarationBeg) &&
									(nexttoken.type != XmlTokenType::TagClosing || lastAppliedTokenType == XmlTokenType::TagOpeningEnd))) {
								lastAppliedTokenType = XmlTokenType::Text;
								this->out.write(token.chars, token.size);
							}
						}
						else {
							lastAppliedTokenType = XmlTokenType::Text;
							this->out << tmp;
						}
					}
					break;
				}
				case XmlTokenType::TagOpeningEnd: {
					if (this->params.ensureConformity) {
						nexttoken = this->parser->getNextToken();
					}
					else {
						nexttoken = this->parser->getNextStructureToken();
					}
					if (this->params.autoCloseTags &&
						nexttoken.type == XmlTokenType::TagClosing) {
						lastAppliedTokenType = XmlTokenType::TagSelfClosingEnd;
						this->out << "/>";
						applyAutoclose = true;
					}
					else {
						lastAppliedTokenType = XmlTokenType::TagOpeningEnd;
						this->out << ">";
						applyAutoclose = false;
					}
					break;
				}
				case XmlTokenType::TagClosing: {	// </ns:sample
					if (!applyAutoclose) {
						lastAppliedTokenType = XmlTokenType::TagClosing;
						this->out.write(token.chars, token.size);
					}
					break;
				}
				case XmlTokenType::TagClosingEnd: {
					if (!applyAutoclose) {
						lastAppliedTokenType = XmlTokenType::TagClosingEnd;
						this->out << ">";
					}
					applyAutoclose = false;
					break;
				}
				case XmlTokenType::TagSelfClosingEnd: {
					lastAppliedTokenType = XmlTokenType::TagSelfClosingEnd;
					this->out << "/>";
					applyAutoclose = false;
					break;
				}
				case XmlTokenType::TagOpening:
				case XmlTokenType::AttrName:
				case XmlTokenType::Comment:
				case XmlTokenType::CDATA:
				case XmlTokenType::DeclarationBeg:
				case XmlTokenType::DeclarationEnd:
				case XmlTokenType::AttrValue:
				case XmlTokenType::Instruction:
				case XmlTokenType::Equal:
				case XmlTokenType::Undefined:
				default: {
					lastAppliedTokenType = token.type;
					this->out.write(token.chars, token.size);
					break;
				}
			}
		}

		return &(this->out);
	}

	std::stringstream* XmlFormater::prettyPrint() {
		this->reset();
		this->parser->reset();

		// the indentOnly mode forces the indentAttributes
		if (this->params.indentOnly) {
			this->params.indentAttributes = true;
		}

		XmlToken token = { XmlTokenType::Undefined }, nexttoken;
		XmlTokenType lastAppliedTokenType = XmlTokenType::Undefined;
		bool lastTextHasLineBreaks = false;
		bool applyAutoclose = false;
		size_t numAttr = 0;
		size_t currTagNameLength = 0;

		while ((token = this->parser->parseNext()).type != XmlTokenType::EndOfFile) {
			switch (token.type) {
				case XmlTokenType::TagOpening: {	// <ns:sample
					currTagNameLength = token.size;
					if (this->params.indentOnly) {
						if (lastTextHasLineBreaks) {
							this->writeIndentation();
						}
					}
					else if (!(lastAppliedTokenType & (XmlTokenType::Text | XmlTokenType::CDATA | XmlTokenType::Undefined))) {
						this->writeEOL();
						this->writeIndentation();
					}
					lastAppliedTokenType = XmlTokenType::TagOpening;
					this->out.write(token.chars, token.size);
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::TagOpeningEnd: {
					numAttr = 0;
					nexttoken = this->parser->getNextToken();
					if (this->params.autoCloseTags && nexttoken.type == XmlTokenType::TagClosing) {
						lastAppliedTokenType = XmlTokenType::TagSelfClosingEnd;
						this->out << "/>";
						applyAutoclose = true;
					}
					else {
						lastAppliedTokenType = XmlTokenType::TagOpeningEnd;
						this->out << ">";
						this->updateIndentLevel(1);
						applyAutoclose = false;
					}
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::TagClosing: {	// </ns:sample
					if (!applyAutoclose) {
						this->updateIndentLevel(-1);
						if (this->params.indentOnly) {
							if (lastTextHasLineBreaks) {
								this->writeIndentation();
							}
						}
						else if (!(lastAppliedTokenType & (XmlTokenType::Text | XmlTokenType::CDATA | XmlTokenType::TagOpeningEnd | XmlTokenType::Undefined))) {
							this->writeEOL();
							this->writeIndentation();
						}
						lastAppliedTokenType = XmlTokenType::TagClosing;
						this->out.write(token.chars, token.size);
					}
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::TagClosingEnd: {
					if (!applyAutoclose) {
						lastAppliedTokenType = XmlTokenType::TagClosingEnd;
						this->out << ">";
					}
					applyAutoclose = false;
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::TagSelfClosingEnd: {
					numAttr = 0; 
					lastAppliedTokenType = XmlTokenType::TagSelfClosingEnd;
					this->out << "/>";
					applyAutoclose = false;
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::AttrName: {
					if (this->params.indentAttributes && numAttr > 0) {
						if (!this->params.indentOnly) {
							this->writeEOL();
						}
						if (!this->params.indentOnly || lastTextHasLineBreaks) {
							this->writeIndentation();
							this->writeElement(" ", currTagNameLength);
						}
					}
					++numAttr;
					this->out << " ";
					lastAppliedTokenType = XmlTokenType::AttrName;
					this->out.write(token.chars, token.size);
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::Text: {
					if (this->params.applySpacePreserve && this->parser->isSpacePreserve()) {
						lastAppliedTokenType = XmlTokenType::Text;
						this->out.write(token.chars, token.size);
					}
					else {
						// check if text could be ignored
						XmlToken nexttoken = this->parser->getNextToken();
						std::string tmp(token.chars, token.size);
						if (this->params.indentOnly) {
							trim_s(tmp);
						}
						else {
							trim(tmp);
						}

						if (tmp.length() > 0 ||
							((!(nexttoken.type & (XmlTokenType::TagOpening | XmlTokenType::Comment | XmlTokenType::DeclarationBeg))) &&
								(nexttoken.type != XmlTokenType::TagClosing || lastAppliedTokenType == XmlTokenType::TagOpeningEnd))) {
							lastAppliedTokenType = XmlTokenType::Text;
							if (this->params.indentOnly) {
								this->out << tmp;
								lastTextHasLineBreaks = (tmp.find_first_of("\r\n") != std::string::npos);
							}
							else {
								this->out.write(token.chars, token.size);
							}
						}
					}
					break;
				}
				case XmlTokenType::LineBreak: {
					if (this->params.applySpacePreserve && this->parser->isSpacePreserve()) {
						lastAppliedTokenType = XmlTokenType::LineBreak;
						this->out.write(token.chars, token.size);
					}
					else if (this->params.indentOnly) {
						lastAppliedTokenType = XmlTokenType::LineBreak;
						this->out.write(token.chars, token.size);
						lastTextHasLineBreaks = true;
					}
					break;
				}
				case XmlTokenType::DeclarationBeg:
				case XmlTokenType::DeclarationSelfClosing: {
					// <!...[
					if (this->params.indentOnly) {
						if (lastTextHasLineBreaks) {
							this->writeIndentation();
						}
					}
					else if (!(lastAppliedTokenType & (XmlTokenType::Text | XmlTokenType::CDATA | XmlTokenType::Undefined))) {
						this->writeEOL();
						this->writeIndentation();
					}
					lastAppliedTokenType = token.type;
					this->out.write(token.chars, token.size);
					if (token.type == XmlTokenType::DeclarationBeg) {
						this->updateIndentLevel(1);
					}
					break;
				}
				case XmlTokenType::DeclarationEnd: {
					// > or ]>
					this->updateIndentLevel(-1);
					if (token.chars[0] == ']') {
						if (!this->params.indentOnly) {
							this->writeEOL();
						}
						this->writeIndentation();
					}
					lastAppliedTokenType = XmlTokenType::DeclarationEnd;
					this->out.write(token.chars, token.size);
					break;
				}
				case XmlTokenType::Comment: {
					if (this->params.indentOnly) {
						if (lastTextHasLineBreaks) {
							this->writeIndentation();
						}
					}
					else if (!(lastAppliedTokenType & (XmlTokenType::Text | XmlTokenType::CDATA | XmlTokenType::Undefined))) {
						this->writeEOL();
						this->writeIndentation();
					}
					lastAppliedTokenType = XmlTokenType::Comment;
					this->out.write(token.chars, token.size);
					lastTextHasLineBreaks = false;
					break;
				}
				case XmlTokenType::Whitespace: {
					if (this->params.applySpacePreserve && this->parser->isSpacePreserve()) {
						lastAppliedTokenType = XmlTokenType::Whitespace;
						this->out.write(token.chars, token.size);
					}
					break;
				}
				case XmlTokenType::CDATA:
				case XmlTokenType::AttrValue:
				case XmlTokenType::Instruction:
				case XmlTokenType::Equal:
				case XmlTokenType::EndOfFile:
				case XmlTokenType::Undefined:
				default: {
					lastAppliedTokenType = token.type;
					this->out.write(token.chars, token.size);
					lastTextHasLineBreaks = false;
					break;
				}
			}
		}

		return &(this->out);
	}

	std::stringstream* XmlFormater::currentPath(size_t position, int xpathMode) {
		this->reset();
		this->parser->reset();

		// for performance optimization, all identity attributes are lowercased

		for (std::vector<std::string>::iterator it = this->params.identityAttribues.begin(); it != this->params.identityAttribues.end(); ++it) {
			*it = to_lowercase(*it);
		}

		XmlToken token = undefinedToken;
		std::vector<std::string> vPath;
		bool pushed_attr = false;
		bool keep_attr_value = false;
		
		// count elements of every depth layer in a map
		std::vector<std::map<std::string, size_t>> depthElementMap;

		while ((token = this->parser->parseNext()).type != XmlTokenType::EndOfFile) {
			if (token.pos >= position) {
				// cursor position reached, let's stop the loops
				break;
			}

			switch (token.type) {
				case XmlTokenType::TagOpening: {
					std::string pathElement = std::string(token.chars + 1, token.size - 1);

					if ((xpathMode & XPATH_MODE_WITHNODEINDEX) != 0) {

						std::map<std::string, size_t> depthMap;
						// Push a new map for the new layer onto the depthElementMap
						depthElementMap.push_back(depthMap);

						if (depthElementMap.size() > 1) {
							// increase amount of elements at current depth
							depthElementMap.at(depthElementMap.size() - 2)[pathElement]++;
							size_t elementsInPosition = depthElementMap.at(depthElementMap.size() - 2)[pathElement];
							pathElement+= "[" + std::to_string(elementsInPosition) + "]";
						}
					}

					vPath.push_back(pathElement);
					pushed_attr = false;
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::TagClosingEnd: {
					if (!vPath.empty()) vPath.pop_back();
					if ((xpathMode & XPATH_MODE_WITHNODEINDEX) != 0) depthElementMap.pop_back();
					pushed_attr = false;
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::TagSelfClosingEnd: {
					if (pushed_attr && !vPath.empty()) vPath.pop_back();
					if ((xpathMode & XPATH_MODE_WITHNODEINDEX) != 0) depthElementMap.pop_back();
					vPath.pop_back();
					pushed_attr = false;
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::AttrName: {
					// We dont need Attributes, when we have an exact Path with Indices to the Node.
					if (xpathMode & XPATH_MODE_WITHNODEINDEX) break;

					if (pushed_attr && !vPath.empty()) {
						vPath.pop_back();
					}
					std::string attr(token.chars, token.size);
					if ((xpathMode & XPATH_MODE_KEEPIDATTRIBUTE) != 0 && isIdentAttribute(attr)) {
						// we must check if attribute is "id"; if true, we must rewrite the
						// tag name and add the value of @id attribute
						keep_attr_value = true;
					}
					vPath.push_back("@" + attr);
					pushed_attr = true;
					break;
				}
				case XmlTokenType::AttrValue: {
					if (keep_attr_value && vPath.size() >= 2) {
						std::string attr = vPath.back();
						vPath.pop_back();
						std::string tag = vPath.back();
						vPath.pop_back();

						if (this->params.dumpIdAttributesName) {
							std::string value(token.chars, token.size);
							if (ends_with(tag, "]")) {
								tag.erase(tag.length() - 1, 1);
								tag += " ";
							}
							else {
								tag += "[";
							}
							tag += attr.substr(1) + "=" + value + "]";
						}
						else {
							std::string value(token.chars+1, token.size-2);
							if (ends_with(tag, "]")) {
								tag.erase(tag.length() - 1, 1);
								tag += " | ";
							}
							else {
								tag += "[";
							}
							tag += value + "]";
						}
						vPath.push_back(tag);
						vPath.push_back(attr);
					}
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::TagOpeningEnd: {
					if (pushed_attr && !vPath.empty()) {
						vPath.pop_back();
					}
					pushed_attr = false;
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::DeclarationBeg:
				case XmlTokenType::DeclarationEnd: {
					// declarations might corrupt the vPath construction
					vPath.clear();
					pushed_attr = false;
					keep_attr_value = false;
					break;
				}
				case XmlTokenType::LineBreak:
				case XmlTokenType::Whitespace:
				case XmlTokenType::Text:
				case XmlTokenType::TagClosing:
				case XmlTokenType::Comment:
				case XmlTokenType::CDATA:
				case XmlTokenType::Instruction:
				case XmlTokenType::Equal:
				case XmlTokenType::Undefined:
				default: {
					break;
				}
			}
		}

		size_t size = vPath.size();
		for (size_t i = 0; i < size; ++i) {
			this->out << "/";
			std::string tmp = vPath.at(i);
			std::string::size_type p = tmp.find(':');
			if ((xpathMode & XPATH_MODE_WITHNAMESPACE) == 0 && tmp.at(0) == '@' && p != std::string::npos) {
				this->out << "@" << tmp.substr(p + 1);
			} else if ((xpathMode & XPATH_MODE_WITHNAMESPACE) == 0 && p != std::string::npos) {
				this->out << tmp.substr(p + 1);
			}
			else {
				this->out << tmp;
			}
		}

		return &(this->out);
	}

	void XmlFormater::writeEOL() {
		this->out << this->params.eolChars;
	}

	void XmlFormater::writeIndentation() {
		for (size_t i = 0; i < this->indentLevel; ++i) {
			this->out << this->params.indentChars;
		}
	}

	void XmlFormater::writeElement(std::string str, size_t num) {
		for (size_t i = 0; i < num; ++i) {
			this->out << str;
		}
	}

	void XmlFormater::updateIndentLevel(int change) {
		if (change > 0) {
			++this->levelCounter;
		}
		else if (change < 0) {
			if (this->levelCounter > 0) {
				--this->levelCounter;
			}
			else {
				this->levelCounter = 0;
			}
		}

		this->indentLevel = this->levelCounter;
		if (this->params.maxIndentLevel > 0 && this->indentLevel > this->params.maxIndentLevel) {
			this->indentLevel = this->params.maxIndentLevel;
		}
	}

	XmlFormaterParamsType XmlFormater::getDefaultParams() {
		XmlFormaterParamsType params;
		params.indentChars = "  ";
		params.eolChars = "\n";
		params.maxIndentLevel = 255;
		params.ensureConformity = true;
		params.autoCloseTags = false;
		params.indentAttributes = false;
		params.indentOnly = false;
		params.applySpacePreserve = false;
		params.dumpIdAttributesName = true;
		return params;
	}
}