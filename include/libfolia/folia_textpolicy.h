/*
  Copyright (c) 2006 - 2021
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of libfolia

  libfolia is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  libfolia is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      https://github.com/LanguageMachines/ticcutils/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
*/

#ifndef FOLIA_TEXTPOLICY_H
#define FOLIA_TEXTPOLICY_H

#include "ticcutils/Unicode.h"

namespace folia {

  class FoliaElement;
  class TextPolicy;

  /// class used to steer 'text()' search behaviour
  /// the values may be logically 'or'-ed, like RETAIN|HIDDEN
  enum class TEXT_FLAGS {
    NONE=0,     //!< None of the flags is set. This is the default.
      RETAIN=1, //!< When returning text, keep al tokenization.
      STRICT=2, /*!< return only text from the current textcontent sibling.
		  The default is NOT STRICT, meaning to get text from deeper
		  textcontent nodes too. (stopping at the first that HAS text)
		*/
      HIDDEN=4,  //!< Include text from 'hidden' nodes.
      NO_TRIM_SPACES=8 /*!< Do not trim leading and trailing spaces (was the default
                          prior to FoLiA v2.4.1, see
                          https://github.com/proycon/folia/issues/92
                        */

      };

  inline TEXT_FLAGS operator&( TEXT_FLAGS f1, TEXT_FLAGS f2 ){
    return (TEXT_FLAGS)((int)f1&(int)f2);
  }

  inline TEXT_FLAGS operator|( TEXT_FLAGS f1, TEXT_FLAGS f2 ){
    return (TEXT_FLAGS) ((int)f1|(int)f2);
  }

  inline TEXT_FLAGS& operator|=( TEXT_FLAGS& f1, TEXT_FLAGS f2 ){
    f1 = (f1 | f2);
    return f1;
  }

  inline TEXT_FLAGS& operator&=( TEXT_FLAGS& f1, TEXT_FLAGS f2 ){
    f1 = (f1 & f2);
    return f1;
  }

  inline TEXT_FLAGS operator~( TEXT_FLAGS f1 ){
    return TEXT_FLAGS( ~(int)f1 );
  }

  typedef icu::UnicodeString (*stringFunctionPointer)( const FoliaElement*,
						       const TextPolicy& );

  class TextPolicy {
  public:
    TextPolicy( const std::string& = "current",
		const TEXT_FLAGS=TEXT_FLAGS::NONE );
    TextPolicy( const TEXT_FLAGS );
    bool is_set( TEXT_FLAGS ) const;
    void set( TEXT_FLAGS );
    void clear( TEXT_FLAGS );
    void add_handler( const std::string&, const stringFunctionPointer& );
    stringFunctionPointer remove_handler( const std::string& );
    stringFunctionPointer get_handler( const std::string& ) const;
    std::string get_class() const { return _class; };
    void set_class( const std::string& c ) { _class = c; };
  private:
    std::string _class;
    TEXT_FLAGS _text_flags;
    std::map<std::string, stringFunctionPointer> _tag_handlers;
  };

} // namespace folia

#endif // FOLIA_TEXTPOLICY_H