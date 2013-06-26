/*
  $Id$
  $URL$

  Copyright (c) 1998 - 2013
  ILK   - Tilburg University
  CLiPS - University of Antwerp
 
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
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/StringOps.h"
#include "ticcutils/XMLtools.h"
#include "libfolia/document.h"
#include "config.h"

using namespace std;
using namespace TiCC;

namespace folia {
  
  string VersionName() { return PACKAGE_STRING; }
  string Version() { return VERSION; }

  ostream& operator<<( ostream& os, const FoliaElement& ae ){
    os << " <" << ae.classname();
    KWargs ats = ae.collectAttributes();
    if ( !ae._id.empty() )
      ats["id"] = ae._id;

    KWargs::const_iterator it = ats.begin();
    while ( it != ats.end() ){
      os << " " << it->first << "='" << it->second << "'";
      ++it;
    }
    os << " > {";
    for( size_t i=0; i < ae.data.size(); ++i ){
      os << "<" << ae.data[i]->classname() << ">,";
    }
    os << "}";
    return os;
  }

  ostream& operator<<( ostream&os, const FoliaElement *ae ){
    if ( !ae )
      os << "nil";
    else
      os << *ae;
    return os;
  }

  FoliaElement::FoliaElement( Document *d ){
    mydoc = d;
    _confidence = -1;
    _element_id = BASE;
    refcount = 0;
    _parent = 0;
    _auth = true;
    _required_attributes = NO_ATT;
    _optional_attributes = NO_ATT;
    _annotation_type = AnnotationType::NO_ANN;
    _annotator_type = UNDEFINED;
    _xmltag = "ThIsIsSoWrOnG";
    occurrences = 0;  //#Number of times this element may occur in its parent (0=unlimited, default=0)
    occurrences_per_set = 1; // #Number of times this element may occur per set (0=unlimited, default=1)
    TEXTDELIMITER = " " ;
    PRINTABLE = true;
    AUTH = true;
  }

  FoliaElement::~FoliaElement( ){
    //  cerr << "delete element " << _xmltag << " *= " << (void*)this << endl;
    for ( size_t i=0; i < data.size(); ++i ){
      if ( data[i]->refcount == 0 ) // probably only for words
	delete data[i];
      else {
	mydoc->keepForDeletion( data[i] );
      }
    }
  }

  xmlNs *FoliaElement::foliaNs() const {
    if ( mydoc )
      return mydoc->foliaNs();
    else
      return 0;
  }

  void FoliaElement::setAttributes( const KWargs& kwargs ){
    Attrib supported = _required_attributes | _optional_attributes;
    // if ( _element_id == Feature_t ){
    //   cerr << "set attributes: " << kwargs << " on " << toString(_element_id) << endl;
    //   cerr << "required = " <<  _required_attributes << endl;
    //   cerr << "optional = " <<  _optional_attributes << endl;
    //   cerr << "supported = " << supported << endl;
    //   cerr << "ID & supported = " << (ID & supported) << endl;
    //   cerr << "ID & _required = " << (ID & _required_attributes ) << endl;
    // }
    if ( mydoc && mydoc->debug > 2 ){
      using TiCC::operator <<;
      cerr << "set attributes: " << kwargs << " on " << toString(_element_id) << endl;
    }
  
    KWargs::const_iterator it = kwargs.find( "generate_id" );
    if ( it != kwargs.end() ) {
      if ( !mydoc ){
	throw runtime_error( "can't generate an ID without a doc" );
      }
      FoliaElement * e = (*mydoc)[it->second];
      if ( e ){
	_id = e->generateId( _xmltag );
      }
      else
	throw ValueError("Unable to generate an id from ID= " + it->second );
    }
    else {
      it = kwargs.find( "id" );
      if ( it != kwargs.end() ) {
	if ( !ID & supported )
	  throw ValueError("ID is not supported for " + classname() );
	else {
	  isNCName( it->second );
	  _id = it->second;
	}
      }
      else
	_id = "";
    }

    it = kwargs.find( "set" );
    string def;
    if ( it != kwargs.end() ) {
      if ( !( (CLASS|SETONLY) & supported ) ){
	throw ValueError("Set is not supported for " + classname());
      }
      else {
	_set = it->second;
      }
      if ( !mydoc ){
	throw ValueError( "Set=" + _set + " is used on a node without a document." );
      }
      if ( !mydoc->isDeclared( _annotation_type, _set ) )
	throw ValueError( "Set " + _set + " is used but has no declaration " +
			  "for " + toString( _annotation_type ) + "-annotation" );
    }
    else if ( mydoc && ( def = mydoc->defaultset( _annotation_type )) != "" ){
      _set = def;
    }
    else
      _set = "";
    
    it = kwargs.find( "class" );
    if ( it == kwargs.end() )
      it = kwargs.find( "cls" );
    if ( it != kwargs.end() ) {
      if ( !( CLASS & supported ) )
	throw ValueError("Class is not supported for " + classname() );
      _class = it->second;
      if ( _element_id != TextContent_t ){
	if ( !mydoc ){
	  throw ValueError( "Class=" + _class + " is used on a node without a document." );
	}
	else if ( _set == "" && 
		  mydoc->defaultset( _annotation_type ) == "" && 
		  mydoc->isDeclared( _annotation_type ) ){
	  throw ValueError( "Class " + _class + " is used but has no default declaration " +
			    "for " + toString( _annotation_type ) + "-annotation" );
	}
      }
    }
    else
      _class = "";

    if ( _element_id != TextContent_t ){
      if ( !_class.empty() && _set.empty() )
	throw ValueError("Set is required for " + classname() + 
			 " class=\"" + _class + "\" assigned without set."  );
    }
    it = kwargs.find( "annotator" );    
    if ( it != kwargs.end() ) {
      if ( !(ANNOTATOR & supported) )
	throw ValueError("Annotator is not supported for " + classname() );
      else {
	_annotator = it->second;
      }
    }
    else if ( mydoc &&
	      (def = mydoc->defaultannotator( _annotation_type, _set )) != "" ){
      _annotator = def;
    }
    else
      _annotator = "";
  
    it = kwargs.find( "annotatortype" );    
    if ( it != kwargs.end() ) {
      if ( ! (ANNOTATOR & supported) )
	throw ValueError("Annotatortype is not supported for " + classname() );
      else {
	_annotator_type = stringTo<AnnotatorType>( it->second );
	if ( _annotator_type == UNDEFINED )
	  throw ValueError( "annotatortype must be 'auto' or 'manual', got '"
			    + it->second + "'" );
      }
    }
    else if ( mydoc &&
	      (def = mydoc->defaultannotatortype( _annotation_type, _set ) ) != ""  ){
      _annotator_type = stringTo<AnnotatorType>( def );
      if ( _annotator_type == UNDEFINED )
	throw ValueError("annotatortype must be 'auto' or 'manual'");
    }
    else
      _annotator_type = UNDEFINED;

        
    it = kwargs.find( "confidence" );
    if ( it != kwargs.end() ) {
      if ( !(CONFIDENCE & supported) )
	throw ValueError("Confidence is not supported for " + classname() );
      else {
	try {
	  _confidence = stringTo<double>(it->second);
	  if ( _confidence < 0 || _confidence > 1.0 )
	    throw ValueError("Confidence must be a floating point number between 0 and 1, got " + TiCC::toString(_confidence) );	    
	}
	catch (...){
	  throw ValueError("invalid Confidence value, (not a number?)");
	}
      }
    }
    else
      _confidence = -1;

    it = kwargs.find( "n" );
    if ( it != kwargs.end() ) {
      if ( !(N & supported) )
	throw ValueError("N is not supported for " + classname() );
      else {
	_n = it->second;
      }
    }
    else
      _n = "";
  
    it = kwargs.find( "datetime" );
    if ( it != kwargs.end() ) {
      if ( !(DATETIME & supported) )
	throw ValueError("datetime is not supported for " + classname() );
      else {
	string time = parseDate( it->second );
	if ( time.empty() )
	  throw ValueError( "invalid datetime string:" + it->second );
	_datetime = time;
      }
    }
    else if ( mydoc &&
	      (def = mydoc->defaultdatetime( _annotation_type, _set )) != "" ){
      _datetime = def;
    }
    else
      _datetime.clear();

    it = kwargs.find( "auth" );
    if ( it != kwargs.end() ){
      _auth = stringTo<bool>( it->second );
    }
    
    if ( mydoc && !_id.empty() )
      mydoc->addDocIndex( this, _id );
    
    addFeatureNodes( kwargs );
  }

  void FoliaElement::addFeatureNodes( const KWargs& kwargs ){
    KWargs::const_iterator it = kwargs.begin();
    while ( it != kwargs.end() ){
      string tag = it->first;
      if ( tag == "head" ){
	// "head" is special because the tag is "headfeature"
	// this to avoid conflicts withe the "head" tag!
	KWargs newa;
	newa["class"] = it->second;
	FoliaElement *tmp = new HeadFeature();
	tmp->setAttributes( newa );
	append( tmp );
      }
      else if ( tag == "actor" || tag == "value"|| tag == "function" ||
		tag == "level" || tag == "time" || tag == "modality" ||
		tag == "synset" || tag == "begindatetime" ||
		tag == "enddatetime" ){
	KWargs newa;
	newa["class"] = it->second;
	FoliaElement *tmp = createElement( mydoc, tag );
	tmp->setAttributes( newa );
	append( tmp );
      }
      ++it;
    }
  }

  KWargs FoliaElement::collectAttributes() const {
    KWargs attribs;
    bool isDefaultSet = true;
    bool isDefaultAnn = true;

    if ( !_id.empty() ){
      attribs["_id"] = _id; // sort "id" as first!
    }
    if ( !_set.empty() &&
	 _set != mydoc->defaultset( _annotation_type ) ){
      isDefaultSet = false;
      attribs["set"] = _set;
    }
    if ( !_class.empty() )
      attribs["class"] = _class;

    if ( !_annotator.empty() &&
	 _annotator != mydoc->defaultannotator( _annotation_type, _set ) ){
      isDefaultAnn = false;
      attribs["annotator"] = _annotator;
    }

    if ( !_datetime.empty() &&
	 _datetime != mydoc->defaultdatetime( _annotation_type, _set ) ){
      attribs["datetime"] = _datetime;
    }
    if ( _annotator_type != UNDEFINED ){
      AnnotatorType at = stringTo<AnnotatorType>( mydoc->defaultannotatortype( _annotation_type, _set ) );
      if ( (!isDefaultSet || !isDefaultAnn) && _annotator_type != at ){
	if ( _annotator_type == AUTO )
	  attribs["annotatortype"] = "auto";
	else if ( _annotator_type == MANUAL )
	  attribs["annotatortype"] = "manual";
      }
    }
  
    if ( _confidence >= 0 )
      attribs["confidence"] = TiCC::toString(_confidence);
  
    if ( !_n.empty() )
      attribs["n"] = _n;

    if ( !AUTH || !_auth )
      attribs["auth"] = "no";

    return attribs;
  }

  string FoliaElement::xmlstring() const{
    // serialize to a string (XML fragment)
    xmlNode *n = xml( true, false );
    xmlSetNs( n, xmlNewNs( n, (const xmlChar *)NSFOLIA.c_str(), 0 ) );
    xmlBuffer *buf = xmlBufferCreate();
    xmlNodeDump( buf, 0, n, 0, 0 );
    string result = (const char*)xmlBufferContent( buf );
    xmlBufferFree( buf );
    xmlFreeNode( n );
    return result;
  }

  string tagToAtt( const FoliaElement* c ){
    string att;
    if ( c->isSubClass( Feature_t ) ){
      att = c->xmltag();
      if ( att == "feat" ){
	// "feat" is a Feature_t too. exclude!
	att = "";
      }
      else if ( att == "headfeature" )
	// "head" is special
	att = "head";
    }
    return att;
  }

  xmlNode *FoliaElement::xml( bool recursive, bool kanon ) const {
    xmlNode *e = XmlNewNode( foliaNs(), _xmltag );
    KWargs attribs = collectAttributes();
    set<FoliaElement *> attribute_elements;
    // nodes that can be represented as attributes are converted to atributes
    // and excluded of 'normal' output.
    vector<FoliaElement*>::const_iterator it=data.begin();
    while ( it != data.end() ){
      string at = tagToAtt( *it );
      if ( !at.empty() ){
	attribs[at] = (*it)->cls();
	attribute_elements.insert( *it );
      }
      ++it;
    }
    addAttributes( e, attribs );
    if ( recursive ){
      // append children:
      // we want make sure that text elements are in the right order, 
      // in front and the 'current' class first
      list<FoliaElement *> textelements;
      list<FoliaElement *> otherelements;
      list<FoliaElement *> commentelements;
      multimap<ElementType, FoliaElement *> otherelementsMap;
      vector<FoliaElement*>::const_iterator it=data.begin();
      while ( it != data.end() ){
	if ( attribute_elements.find(*it) == attribute_elements.end() ){
	  if ( (*it)->isinstance(TextContent_t) ){
	    if ( (*it)->_class == "current" )
	      textelements.push_front( *it );
	    else
	      textelements.push_back( *it );
	  }
	  else {
	    if ( kanon )
	      otherelementsMap.insert( make_pair( (*it)->element_id(), *it ) );
	    else {
	      if ( (*it)->isinstance(XmlComment_t) && textelements.empty() ){
		commentelements.push_back( *it );
	      }
	      else
		otherelements.push_back( *it );
	    }
	  }
	}
	++it;
      }
      list<FoliaElement*>::const_iterator lit=commentelements.begin();
      while ( lit != commentelements.end() ){
	xmlAddChild( e, (*lit)->xml( recursive, kanon ) );
	++lit;
      }
      lit=textelements.begin();
      while ( lit != textelements.end() ){
	xmlAddChild( e, (*lit)->xml( recursive, kanon ) );
	++lit;
      }
      if ( !kanon ){
	list<FoliaElement*>::const_iterator lit=otherelements.begin();
	while ( lit != otherelements.end() ){
	  xmlAddChild( e, (*lit)->xml( recursive, kanon ) );
	  ++lit;
	}
      }
      else {
	multimap<ElementType, FoliaElement*>::const_iterator lit
	  = otherelementsMap.begin();
	while ( lit != otherelementsMap.end() ){
	  xmlAddChild( e, (lit->second)->xml( recursive, kanon ) );
	  ++lit;
	}
      }
    }
    return e;
  }

  string FoliaElement::str() const {
    return _xmltag;
  }

  bool FoliaElement::hastext( const string& cls ) const {
    // does this element have a TextContent with class 'cls'
    // Default is class="current"
    try {
      this->textcontent(cls);
      return true;
    } catch (NoSuchText& e ) {
      return false;
    }
  }
  
  UnicodeString FoliaElement::text( const string& cls, bool retaintok ) const {
    // get the UnicodeString value of underlying elements
    // default cls="current"
    if ( !PRINTABLE )
      throw NoSuchText( _xmltag );
    //  cerr << (void*)this << ":text() for " << _xmltag << " and class= " << cls << " step 1 " << endl;
    
    UnicodeString result;    
    try {
      result = this->textcontent(cls)->text(cls, retaintok );
    } catch (NoSuchText& e ) {
      for( size_t i=0; i < data.size(); ++i ){
	// try to get text dynamically from children
	// skip TextContent elements
	if ( data[i]->PRINTABLE && !data[i]->isinstance( TextContent_t ) ){
	  try {
	    UnicodeString tmp = data[i]->text( cls, retaintok );
	    //	cerr << "text() for " << _xmltag << " step 2, tmp= " << tmp << endl;
	    result += tmp;
	    if ( !tmp.isEmpty() ){
	      result += UTF8ToUnicode( data[i]->getTextDelimiter( retaintok ) );
	    }
	  } catch ( NoSuchText& e ){
	  }
	}
      }
    }
    
    //  cerr << "text() for " << _xmltag << " step 3 >> " << endl;
    
    result.trim();
    if ( !result.isEmpty() ){
      //    cerr << "text() for " << _xmltag << " result= " << result << endl;
      return result;
    }
    else
      throw NoSuchText( ": empty!" );
  }
  
  UnicodeString FoliaElement::stricttext( const string& cls ) const {
    // get UnicodeString content of TextContent children only
    // default cls="current"
    return this->textcontent(cls)->text(cls);
  }

  TextContent *FoliaElement::textcontent( const string& cls ) const {
    // Get the text explicitly associated with this element (of the specified class).
    // the default class is 'current'
    // Returns the TextContent instance rather than the actual text. Does not recurse into children
    // with sole exception of Correction
    // Raises NoSuchText exception if not found. 
    
    if ( !PRINTABLE )
      throw NoSuchText( _xmltag );
    
    for( size_t i=0; i < data.size(); ++i ){
      if ( data[i]->isinstance(TextContent_t) && (data[i]->_class == cls) ){
	return dynamic_cast<TextContent*>(data[i]);
      }
      else if ( data[i]->element_id() == Correction_t) {
	try {	    
	  return data[i]->textcontent(cls);
	} catch ( NoSuchText& e ){
	  // continue search for other Corrections or a TextContent
	}
      }
    }    
    throw NoSuchText( "textcontent()" );
  }

  vector<FoliaElement *>FoliaElement::findreplacables( FoliaElement *par ) const {
    return par->select( element_id(), _set, false );
  }

  void FoliaElement::replace( FoliaElement *child ){
    // Appends a child element like append(), but replaces any existing child 
    // element of the same type and set. 
    // If no such child element exists, this will act the same as append()
  
    vector<FoliaElement*> replace = child->findreplacables( this );
    if ( replace.empty() ){
      // nothing to replace, simply call append
      append( child );
    }
    else if ( replace.size() > 1 ){
      throw runtime_error( "Unable to replace. Multiple candidates found, unable to choose." );
    }
    else {
      remove( replace[0], true );
      append( child );
    }
  }                

  TextContent *FoliaElement::settext( const string& txt, 
				      const string& cls ){
    // create a TextContent child of class 'cls'
    // Default cls="current"
    KWargs args;
    args["value"] = txt;
    args["class"] = cls;
    TextContent *node = new TextContent( mydoc );
    node->setAttributes( args );
    replace( node );
    return node;
  }

  TextContent *FoliaElement::settext( const string& txt, 
				      int offset,
				      const string& cls ){
    // create a TextContent child of class 'cls'
    // Default cls="current"
    // sets the offset attribute.
    KWargs args;
    args["value"] = txt;
    args["class"] = cls;
    args["offset"] = TiCC::toString(offset);
    TextContent *node = new TextContent( mydoc );
    node->setAttributes( args );
    replace( node );
    return node;
  }

  string FoliaElement::description() const {
    vector<FoliaElement *> v =  select( Description_t, false );
    if ( v.size() == 0 )
      throw NoDescription();
    else
      return v[0]->description();
  }

  bool isSubClass( const ElementType e1, const ElementType e2 ){
    static map<ElementType,set<ElementType> > sm;
    static ElementType structureSet[] = { Head_t, Division_t,
					  TableHead_t, Table_t, 
					  Row_t, Cell_t, 
					  LineBreak_t, WhiteSpace_t,
					  Word_t, WordReference_t, 
					  Sentence_t, Paragraph_t, 
					  Quote_t, Morpheme_t,
					  Text_t, Event_t, 
					  Caption_t, Label_t,
					  ListItem_t, List_t,
					  Figure_t, Alternative_t };
    sm[Structure_t] 
      = set<ElementType>( structureSet, 
			  structureSet + sizeof(structureSet)/sizeof(ElementType) );
    static ElementType featureSet[] = { SynsetFeature_t, 
					ActorFeature_t, HeadFeature_t, 
					ValueFeature_t, TimeFeature_t, 
					ModalityFeature_t, LevelFeature_t,
					BeginDateTimeFeature_t, 
					EndDateTimeFeature_t,
					FunctionFeature_t };
    sm[Feature_t] 
      = set<ElementType>(featureSet, 
			 featureSet + sizeof(featureSet)/sizeof(ElementType) );
    static ElementType tokenAnnoSet[] = { Pos_t, Lemma_t, Morphology_t,
					  Sense_t, Phon_t, Str_t, Lang_t,
					  Correction_t, Subjectivity_t,
					  ErrorDetection_t };
    sm[TokenAnnotation_t] 
      = set<ElementType>( tokenAnnoSet, 
			  tokenAnnoSet + sizeof(tokenAnnoSet)/sizeof(ElementType) );
    static ElementType annolaySet[] = { SyntaxLayer_t, 
					Chunking_t, Entities_t, 
					TimingLayer_t, Morphology_t,
					Dependencies_t, 
					Coreferences_t, Semroles_t };
    sm[AnnotationLayer_t] 
      = set<ElementType>( annolaySet, 
			  annolaySet + sizeof(annolaySet)/sizeof(ElementType) );
    
    if ( e1 == e2 )
      return true;
    map<ElementType,set<ElementType> >::const_iterator it = sm.find( e2 );
    if ( it != sm.end() ){
      return it->second.find( e1 ) != it->second.end();
    }
    return false;
  }

  bool FoliaElement::isSubClass( ElementType t ) const {
    return folia::isSubClass( _element_id, t );
  }
  
  bool FoliaElement::acceptable( ElementType t ) const {
    if ( t == XmlComment_t )
      return true;
    else {
      set<ElementType>::const_iterator it = _accepted_data.find( t );
      if ( it == _accepted_data.end() ){
	it = _accepted_data.begin();
	while ( it != _accepted_data.end() ){
	  if ( folia::isSubClass( t, *it ) )
	    return true;
	  ++it;
	}
	return false;
      }
      return true;
    }
  }
 
  bool FoliaElement::addable( const FoliaElement *c ) const {
    if ( !acceptable( c->_element_id ) ){
      throw ValueError( "Unable to append object of type " + c->classname()
			+ " to a " + classname() );
    }
    if ( c->occurrences > 0 ){
      vector<FoliaElement*> v = select( c->_element_id );
      size_t count = v.size();
      if ( count > c->occurrences ){
	throw DuplicateAnnotationError( "Unable to add another object of type " + c->classname() + " to " + classname() + ". There are already " + TiCC::toString(count) + " instances of this class, which is the maximum." );
      }
    }
    if ( c->occurrences_per_set > 0 &&
	 ( (CLASS|SETONLY) & c->_required_attributes ) ){
      vector<FoliaElement*> v = select( c->_element_id, c->_set );
      size_t count = v.size();
      if ( count > c->occurrences_per_set )
	throw DuplicateAnnotationError( "Unable to add another object of type " + c->classname() + " to " + classname() + ". There are already " + TiCC::toString(count) + " instances of this class, which is the maximum." );
    }
    return true;
  }
 
  void FoliaElement::fixupDoc( Document* doc ) {
    if ( !mydoc ){
      mydoc = doc;
      string myid = id();
      if ( !_set.empty() 
	   && (CLASS & _required_attributes )
	   && !mydoc->isDeclared( _annotation_type, _set ) )
	throw ValueError( "Set " + _set + " is used in " + _xmltag 
			  + "element: " + myid + " but has no declaration " +
			  "for " + toString( _annotation_type ) + "-annotation" );
      if ( !myid.empty() )
	doc->addDocIndex( this, myid );
      // assume that children also might be doc-less
      for( size_t i=0; i < data.size(); ++i )
	data[i]->fixupDoc( doc );
    }
  }
  
  bool FoliaElement::checkAtts(){
    if ( _id.empty() && (ID & _required_attributes ) ){
      throw ValueError( "ID is required for " + classname() );
    }
    if ( _set.empty() && (CLASS & _required_attributes ) ){
      throw ValueError( "Set is required for " + classname() );
    }
    if ( _class.empty() && ( CLASS & _required_attributes ) ){
      throw ValueError( "Class is required for " + classname() );
    }
    if ( _annotator.empty() && ( ANNOTATOR & _required_attributes ) ){
      throw ValueError( "Annotator is required for " + classname() );
    }
    if ( _annotator_type == UNDEFINED && ( ANNOTATOR & _required_attributes ) ){
      throw ValueError( "Annotatortype is required for " + classname() );
    }
    if ( _confidence == -1 && ( CONFIDENCE & _required_attributes ) ){
      throw ValueError( "Confidence is required for " + classname() );
    }
    if ( _n.empty() && ( N & _required_attributes ) ){
      throw ValueError( "N is required for " + classname() );
    }
    if ( DATETIME & _required_attributes ){
      throw ValueError( "datetime is required for " + classname() );
    }
    return true;
  }

  FoliaElement *FoliaElement::append( FoliaElement *child ){
    bool ok = false;
    try {
      ok = child->checkAtts();
      ok = addable( child );
    }
    catch ( exception& ){
      delete child;
      throw;
    }
    if ( ok ){
      child->fixupDoc( mydoc );
      data.push_back(child);
      if ( !child->_parent ) // Only for WordRef i hope
	child->_parent = this;
      return child->postappend();
    }
    return 0;
  }


  void FoliaElement::remove( FoliaElement *child, bool del ){
    vector<FoliaElement*>::iterator it = std::remove( data.begin(), data.end(), child );
    data.erase( it, data.end() );
    if ( del )
      delete child;
  }

  FoliaElement* FoliaElement::index( size_t i ) const {
    if ( i < data.size() )
      return data[i];
    else
      throw range_error( "[] index out of range" );
  }

  FoliaElement* FoliaElement::rindex( size_t i ) const {
    if ( i < data.size() )
      return data[data.size()-1-i];
    else
      throw range_error( "[] rindex out of range" );
  }

  vector<FoliaElement*> FoliaElement::select( ElementType et,
					      const string& st,
					      const set<ElementType>& exclude,
					      bool recurse ) const {
    vector<FoliaElement*> res;
    for ( size_t i = 0; i < data.size(); ++i ){
      if ( data[i]->_element_id == et && 
	   ( st.empty() || data[i]->_set == st ) ){
	res.push_back( data[i] );
      }
      if ( recurse ){
	if ( exclude.find( data[i]->_element_id ) == exclude.end() ){
	  vector<FoliaElement*> tmp = data[i]->select( et, st, exclude, recurse );
	  res.insert( res.end(), tmp.begin(), tmp.end() );
	}
      }
    }
    return res;
  }
  
  static ElementType ignoreList[] = { Original_t,
				      Suggestion_t, 
				      Alternative_t };
  set<ElementType> default_ignore( ignoreList,
				   ignoreList + sizeof( ignoreList )/sizeof( ElementType ) );
  
  static ElementType ignoreAnnList[] = { Original_t,
					 Suggestion_t, 
					 Alternative_t,
					 Morphology_t };
  set<ElementType> default_ignore_annotations( ignoreAnnList,
					       ignoreAnnList + sizeof( ignoreAnnList )/sizeof( ElementType ) );
  
  static ElementType ignoreStrList[] = { Original_t,
					 Suggestion_t, 
					 Alternative_t,
					 Chunk_t, 
					 SyntacticUnit_t,
					 Coreferences_t,
					 Semroles_t,
					 Entity_t,
					 Headwords_t,
					 TimingLayer_t,
					 DependencyDependent_t,
					 TimeSegment_t };
  set<ElementType> default_ignore_structure( ignoreStrList,
					     ignoreStrList + sizeof( ignoreStrList )/sizeof( ElementType ) );
  

  vector<FoliaElement*> FoliaElement::select( ElementType et,
					      const string& st,
					      bool recurse ) const {
    return select( et, st, default_ignore, recurse );
  }

  vector<FoliaElement*> FoliaElement::select( ElementType et,
					      const set<ElementType>& exclude,
					      bool recurse ) const {
    vector<FoliaElement*> res;
    for ( size_t i = 0; i < data.size(); ++i ){
      if ( data[i]->_element_id == et ){
	res.push_back( data[i] );
      }
      if ( recurse ){
	if ( exclude.find( data[i]->_element_id ) == exclude.end() ){
	  vector<FoliaElement*> tmp = data[i]->select( et, exclude, recurse );
	  res.insert( res.end(), tmp.begin(), tmp.end() );
	}
      }
    }
    return res;
  }

  vector<FoliaElement*> FoliaElement::select( ElementType et,
					      bool recurse ) const {
    return select( et, default_ignore, recurse );
  }
  
  FoliaElement* FoliaElement::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    //    cerr << "got attributes " << att << endl;
    setAttributes( att );
    xmlNode *p = node->children;
    while ( p ){
      if ( p->type == XML_ELEMENT_NODE ){
	string tag = Name( p );
	FoliaElement *t = createElement( mydoc, tag );
	if ( t ){
	  if ( mydoc && mydoc->debug > 2 )
	    cerr << "created " << t << endl;
	  t = t->parseXml( p );
	  if ( t ){
	    if ( mydoc && mydoc->debug > 2 )
	      cerr << "extend " << this << " met " << tag << endl;
	    append( t );
	  }
	}
      }
      if ( p->type == XML_COMMENT_NODE ){
	string tag = "xml-comment";
	FoliaElement *t = createElement( mydoc, tag );
	if ( t ){
	  if ( mydoc && mydoc->debug > 2 )
	    cerr << "created " << t << endl;
	  t = t->parseXml( p );
	  if ( t ){
	    if ( mydoc && mydoc->debug > 2 )
	      cerr << "extend " << this << " met " << tag << endl;
	    append( t );
	  }
	}
      }
      p = p->next;
    }
    return this;
  }

  void FoliaElement::setDateTime( const string& s ){
    Attrib supported = _required_attributes | _optional_attributes;
    if ( !(DATETIME & supported) )
      throw ValueError("datetime is not supported for " + classname() );
    else {
      string time = parseDate( s );
      if ( time.empty() )
	throw ValueError( "invalid datetime string:" + s );
      _datetime = time;
    }
  }

  string FoliaElement::getDateTime() const {
    return _datetime;
  }

  string FoliaElement::pos( const string& st ) const { 
    return annotation<PosAnnotation>( st )->cls(); 
  }
  
  string FoliaElement::lemma( const string& st ) const { 
    return annotation<LemmaAnnotation>( st )->cls(); 
  }

  PosAnnotation *FoliaElement::addPosAnnotation( const KWargs& args ){
    return addAnnotation<PosAnnotation>( args );
  }

  LemmaAnnotation *FoliaElement::addLemmaAnnotation( const KWargs& args ){
    return addAnnotation<LemmaAnnotation>( args );
  }

  Sentence *FoliaElement::addSentence( const KWargs& args ){
    Sentence *res = new Sentence( mydoc );
    KWargs kw = args;
    if ( kw["id"].empty() ){
      string id = generateId( "s" );
      kw["id"] = id;
    }
    try {
      res->setAttributes( kw );
    }
    catch( DuplicateIDError& e ){
      delete res;
      throw e;
    }
    append( res );
    return res;
  }

  Word *FoliaElement::addWord( const KWargs& args ){
    Word *res = new Word( mydoc );
    KWargs kw = args;
    if ( kw["id"].empty() ){
      string id = generateId( "w" );
      kw["id"] = id;
    }
    try {
      res->setAttributes( kw );
    }
    catch( DuplicateIDError& e ){
      delete res;
      throw e;
    }
    append( res );
    return res;
  }


  vector<Word*> Quote::wordParts() const {
    vector<Word*> result;
    for ( size_t i=0; i < data.size(); ++i ){
      FoliaElement *pnt = data[i];
      if ( pnt->isinstance( Word_t ) )
	result.push_back( dynamic_cast<Word*>(pnt) );
      else if ( pnt->isinstance( Sentence_t ) ){
	PlaceHolder *p = new PlaceHolder( mydoc, "text='" + 
					  pnt->id() + "'");
	mydoc->keepForDeletion( p );
	result.push_back( p );
      }
      else if ( pnt->isinstance( Quote_t ) ){
	vector<Word*> tmp = pnt->wordParts();
	result.insert( result.end(), tmp.begin(), tmp.end() );
      }
      else if ( pnt->isinstance( Description_t ) ){
	// ignore
      }
      else {
	throw XmlError( "Word or Sentence expected in Quote. got: " 
			+ pnt->classname() );
      }
    }
    return result;
  }

  vector<Word*> Sentence::wordParts() const {
    vector<Word*> result;
    for ( size_t i=0; i < data.size(); ++i ){
      FoliaElement *pnt = data[i];
      if ( pnt->isinstance( Word_t ) )
	result.push_back( dynamic_cast<Word*>(pnt) );
      else if ( pnt->isinstance( Quote_t ) ){	
	vector<Word*> v = pnt->wordParts();
	result.insert( result.end(), v.begin(),v.end() );
      }
      else {
	// skip all other stuff. Is there any?
      }
    }
    return result;
  }
  
  Correction *Sentence::splitWord( FoliaElement *orig, FoliaElement *p1, FoliaElement *p2, const KWargs& args ){
    vector<FoliaElement*> ov;
    ov.push_back( orig );
    vector<FoliaElement*> nv;
    nv.push_back( p1 );
    nv.push_back( p2 );
    vector<FoliaElement*> nil;
    return correctWords( ov, nv, nil, args );
  }

  Correction *Sentence::mergewords( FoliaElement *nw, 
				    const vector<FoliaElement *>& orig,
				    const string& args ){
    vector<FoliaElement*> nv;
    nv.push_back( nw );
    vector<FoliaElement*> nil;
    return correctWords( orig, nv, nil, getArgs(args) );
  }

  Correction *Sentence::deleteword( FoliaElement *w, 
				    const string& args ){
    vector<FoliaElement*> ov;
    ov.push_back( w );
    vector<FoliaElement*> nil;
    return correctWords( ov, nil, nil, getArgs(args) );
  }

  Correction *Sentence::insertword( FoliaElement *w, 
				    FoliaElement *p,
				    const string& args ){
    if ( !p || !p->isinstance( Word_t ) )
      throw runtime_error( "insertword(): previous is not a Word " );
    if ( !w || !w->isinstance( Word_t ) )
      throw runtime_error( "insertword(): new word is not a Word " );
    Word *tmp = new Word( "text='dummy', id='dummy'" );
    tmp->setParent( this ); // we create a dummy Word as member of the
    // Sentence. This makes correct() happy
    vector<FoliaElement *>::iterator it = data.begin();
    while ( it != data.end() ){
      if ( *it == p ){
	it = data.insert( ++it, tmp );
	break;
      }
      ++it;
    }
    if ( it == data.end() )
      throw runtime_error( "insertword(): previous not found" );
    vector<FoliaElement *> ov;
    ov.push_back( *it );
    vector<FoliaElement *> nv;
    nv.push_back( w );
    vector<FoliaElement*> nil;
    return correctWords( ov, nv, nil, getArgs(args) );  
  }

  Correction *Sentence::correctWords( const vector<FoliaElement *>& orig,
				      const vector<FoliaElement *>& _new,
				      const vector<FoliaElement *>& current, 
				      const KWargs& args ){
    // Generic correction method for words. You most likely want to use the helper functions
    //      splitword() , mergewords(), deleteword(), insertword() instead
  
    // sanity check:
    vector<FoliaElement *>::const_iterator it = orig.begin();
    while ( it != orig.end() ){
      if ( !(*it) || !(*it)->isinstance( Word_t) )
	throw runtime_error("Original word is not a Word instance" );
      else if ( (*it)->sentence() != this )
	throw runtime_error( "Original not found as member of sentence!");
      ++it;
    }
    it = _new.begin();
    while ( it != _new.end() ){
      if ( ! (*it)->isinstance( Word_t) )
	throw runtime_error("new word is not a Word instance" );
      ++it;
    }
    it = current.begin();
    while ( it != current.end() ){
      if ( ! (*it)->isinstance( Word_t) )
	throw runtime_error("current word is not a Word instance" );
      ++it;
    }
    KWargs::const_iterator ait = args.find("suggest");
    if ( ait != args.end() && ait->second == "true" ){
      FoliaElement *sugg = new Suggestion();
      it = _new.begin();
      while ( it != _new.end() ){
	sugg->append( *it );
	++it;
      }
      vector<FoliaElement *> nil;
      vector<FoliaElement *> sv;
      sv.push_back( sugg );
      return correct( nil, orig, nil, sv, args );
    }
    else {
      vector<FoliaElement *> nil;
      return correct( orig, nil, _new, nil, args );
    }
  }

  void TextContent::setAttributes( const KWargs& args ){
    KWargs kwargs = args; // need to copy
    KWargs::const_iterator it = kwargs.find( "value" );
    if ( it != kwargs.end() ) {
      _text = UTF8ToUnicode(it->second);
      kwargs.erase("value");
    }
    else
      throw ValueError( "TextContent expects 'value' attribute" );
    it = kwargs.find( "offset" );
    if ( it != kwargs.end() ) {
      _offset = stringTo<int>(it->second);
      kwargs.erase("offset");
    }
    else
      _offset = -1;
    it = kwargs.find( "ref" );
    if ( it != kwargs.end() ) {
      throw NotImplementedError( "ref attribute in TextContent" );
    }
    it = kwargs.find( "cls" );
    if ( it == kwargs.end() ) 
      it = kwargs.find( "class" );
    if ( it == kwargs.end() ) {
      kwargs["class"] = "current";
    }
    FoliaElement::setAttributes(kwargs);
  }

  FoliaElement* TextContent::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    att["value"] = XmlContent( node );
    setAttributes( att );
    if ( mydoc && mydoc->debug > 2 )
      cerr << "set textcontent to " << _text << endl;
    return this;
  }

  TextContent *TextContent::postappend(){
    if ( _parent->isinstance( Original_t ) ){
      if ( _class == "current" )
	_class = "original";
    }
    return this;
  }

  vector<FoliaElement *>TextContent::findreplacables( FoliaElement *par ) const {
    vector<FoliaElement *> result;
    vector<TextContent*> v = par->select<TextContent>( _set, false );
    // cerr << "TextContent::findreplacable found " << v << endl;
    vector<TextContent*>::iterator it = v.begin();
    while ( it != v.end() ){
      // cerr << "TextContent::findreplacable bekijkt " << *it << " (" 
      if ( (*it)->cls() == _class )
	result.push_back( *it );
      ++it;
    }
    //  cerr << "TextContent::findreplacable resultaat " << v << endl;
    return result;
  }


  string TextContent::str() const{
    return UnicodeToUTF8(_text);
  }

  UnicodeString TextContent::text( const string&, bool ) const{
    return _text;
  }

  string AllowGenerateID::IGgen( const string& tag, 
				 const string& nodeId ){
    //    cerr << "generateId," << tag << " nodeId = " << nodeId << endl;
    int max = getMaxId(tag);
    //    cerr << "MAX = " << max << endl;
    string id = nodeId + '.' + tag + '.' +  TiCC::toString( max + 1 );
    //    cerr << "new id = " << id << endl;
    return id;
  }

  void AllowGenerateID::setMaxId( FoliaElement *child ) {
    if ( !child->id().empty() && !child->xmltag().empty() ){
      vector<string> parts;
      size_t num = TiCC::split_at( child->id(), parts, "." );
      if ( num > 0 ){
	string val = parts[num-1];
	int i;
	try {
	  i = stringTo<int>( val );
	}
	catch ( exception ){
	  // no number, so assume so user defined id
	  return;
	}
	map<string,int>::iterator it = maxid.find( child->xmltag() );
	if ( it == maxid.end() ){
	  maxid[child->xmltag()] = i;
	}
	else {
	  if ( it->second < i ){
	    it->second = i;
	  }
	}
      }
    }
  }

  int AllowGenerateID::getMaxId( const string& xmltag ) {
    int res = 0;
    if ( !xmltag.empty() ){
      res = maxid[xmltag];
      ++maxid[xmltag];
    }
    return res;
  }
 
  Correction * AllowCorrection::correctBase( FoliaElement *root, 
					     vector<FoliaElement*> original,
					     vector<FoliaElement*> current,
					     vector<FoliaElement*> _new,
					     vector<FoliaElement*> suggestions,
					     const KWargs& args_in ){
    // cerr << "correct " << this << endl;
    // cerr << "original= " << original << endl;
    // cerr << "current = " << current << endl;
    // cerr << "new     = " << _new << endl;
    // cerr << "suggestions     = " << suggestions << endl;
    //  cerr << "args in     = " << args_in << endl;
    // Apply a correction
    Document *mydoc = root->doc();
    Correction *c = 0;
    bool suggestionsonly = false;
    bool hooked = false;
    FoliaElement * addnew = 0;
    KWargs args = args_in;
    KWargs::const_iterator it = args.find("new");
    if ( it != args.end() ){
      TextContent *t = new TextContent( mydoc, "value='" +  it->second + "'" );
      _new.push_back( t );
      args.erase("new");
    }
    it = args.find("suggestion");
    if ( it != args.end() ){
      TextContent *t = new TextContent( mydoc, "value='" +  it->second + "'" );
      suggestions.push_back( t );
      args.erase("suggestion");
    }
    it = args.find("reuse");
    if ( it != args.end() ){
      // reuse an existing correction instead of making a new one
      try {
	c = dynamic_cast<Correction*>(mydoc->index(it->second)); 
      }
      catch ( exception& e ){
	throw ValueError("reuse= must point to an existing correction id!");
      }
      if ( !c->isinstance( Correction_t ) ){
	throw ValueError("reuse= must point to an existing correction id!");
      }
      hooked = true;
      suggestionsonly = (!c->hasNew() && !c->hasOriginal() && c->hasSuggestions() );
      if ( !_new.empty() && c->hasCurrent() ){
	// can't add new if there's current, so first set original to current, and then delete current
      
	if ( !current.empty() )
	  throw runtime_error( "Can't set both new= and current= !");
	if ( original.empty() ){
	  FoliaElement *cur = c->getCurrent();
	  original.push_back( cur );
	  c->remove( cur, false );
	}
      }
    }
    else {
      KWargs args2 = args;
      args2.erase("suggestion" );
      args2.erase("suggestions" );
      string id = root->generateId( "correction" );
      args2["id"] = id;
      c = new Correction(mydoc );
      c->setAttributes( args2 );
    }
  
    if ( !current.empty() ){
      if ( !original.empty() || !_new.empty() )
	throw runtime_error("When setting current=, original= and new= can not be set!");
      vector<FoliaElement *>::iterator cit = current.begin();
      while ( cit != current.end() ){
	FoliaElement *add = new Current( mydoc );
	add->append( *cit );
	c->replace( add );
	if ( !hooked ) {
	  for ( size_t i=0; i < root->data.size(); ++i ){
	    if ( root->data[i] == *cit ){
	      //	    delete data[i];
	      root->data[i] = c;
	      hooked = true;
	    }
	  }
	}
	++cit;
      }
    }
    if ( !_new.empty() ){
      //    cerr << "there is new! " << endl;
      addnew = new NewElement( mydoc );
      c->append(addnew);
      vector<FoliaElement *>::iterator nit = _new.begin();    
      while ( nit != _new.end() ){
	addnew->append( *nit );
	++nit;
      }
      //    cerr << "after adding " << c << endl;
      vector<Current*> v = c->select<Current>();
      //delete current if present
      vector<Current*>::iterator cit = v.begin();    
      while ( cit != v.end() ){
	c->remove( *cit, false );
	++cit;
      }
    }
    if ( !original.empty() ){
      FoliaElement *add = new Original( mydoc );
      c->replace(add);
      vector<FoliaElement *>::iterator nit = original.begin();
      while ( nit != original.end() ){
	bool dummyNode = ( (*nit)->id() == "dummy" );
	if ( !dummyNode )
	  add->append( *nit );
	for ( size_t i=0; i < root->data.size(); ++i ){
	  if ( root->data[i] == *nit ){
	    if ( !hooked ) {
	      if ( dummyNode )
		delete root->data[i];
	      root->data[i] = c;
	      hooked = true;
	    }
	    else 
	      root->remove( *nit, false );
	  }
	}
	++nit;
      }
    }
    else if ( addnew ){
      // original not specified, find automagically:
      vector<FoliaElement *> orig;
      //    cerr << "start to look for original " << endl;
      for ( size_t i=0; i < len(addnew); ++ i ){
	FoliaElement *p = addnew->index(i);
	//      cerr << "bekijk " << p << endl;
	vector<FoliaElement*> v = p->findreplacables( root );
	vector<FoliaElement*>::iterator vit=v.begin();      
	while ( vit != v.end() ){
	  orig.push_back( *vit );
	  ++vit;
	}
      }
      if ( orig.empty() ){
	throw runtime_error( "No original= specified and unable to automatically infer");
      }
      else {
	//      cerr << "we seem to have some originals! " << endl;
	FoliaElement *add = new Original( mydoc );
	c->replace(add);
	vector<FoliaElement *>::iterator oit = orig.begin();
	while ( oit != orig.end() ){
	  //	cerr << " an original is : " << *oit << endl;
	  add->append( *oit );
	  for ( size_t i=0; i < root->data.size(); ++i ){
	    if ( root->data[i] == *oit ){
	      if ( !hooked ) {
		//delete data[i];
		root->data[i] = c;
		hooked = true;
	      }
	      else 
		root->remove( *oit, false );
	    }
	  }
	  ++oit;
	}
	vector<Current*> v = c->select<Current>();
	vector<Current*>::iterator cit = v.begin();
	//delete current if present
	while ( cit != v.end() ){
	  root->remove( *cit, false );
	  ++cit;
	}
      }
    }
  
    if ( addnew ){
      vector<FoliaElement*>::iterator oit = original.begin();
      while ( oit != original.end() ){
	c->remove( *oit, false );
	++oit;
      }
    }

    if ( !suggestions.empty() ){
      if ( !hooked )
	root->append(c);
      vector<FoliaElement *>::iterator nit = suggestions.begin();
      while ( nit != suggestions.end() ){
	if ( (*nit)->isinstance( Suggestion_t ) ){
	  c->append( *nit );
	}
	else {
	  FoliaElement *add = new Suggestion( mydoc );
	  add->append( *nit );
	  c->append( add );
	}
	++nit;
      }
    }
  
    it = args.find("reuse");
    if ( it != args.end() ){
      if ( addnew && suggestionsonly ){
	vector<Suggestion*> sv = c->suggestions();
	for ( size_t i=0; i < sv.size(); ++i ){
	  if ( !c->annotator().empty() && sv[i]->annotator().empty() )
	    sv[i]->annotator( c->annotator() );
	  if ( !(c->annotatortype() == UNDEFINED) && 
	       (sv[i]->annotatortype() == UNDEFINED ) )
	    sv[i]->annotatortype( c->annotatortype() );
	}
      }
      it = args.find("annotator");
      if ( it != args.end() )
	c->annotator( it->second );
      it = args.find("annotatortype");
      if ( it != args.end() )
	c->annotatortype( stringTo<AnnotatorType>(it->second) );
      it = args.find("confidence");
      if ( it != args.end() )
	c->confidence( stringTo<double>(it->second) );

    }
    return c;
  }

  string AbstractStructureElement::str() const{
    UnicodeString result = text();
    return UnicodeToUTF8(result);
  }
  
  FoliaElement *AbstractStructureElement::append( FoliaElement *child ){
    FoliaElement::append( child );
    setMaxId( child );
    return child;
  }

  vector<Paragraph*> AbstractStructureElement::paragraphs() const{
    return select<Paragraph>( default_ignore_structure );
  }

  vector<Sentence*> AbstractStructureElement::sentences() const{
    return select<Sentence>( default_ignore_structure );
  }

  vector<Word*> AbstractStructureElement::words() const{
    return select<Word>( default_ignore_structure );
  }

  Sentence *AbstractStructureElement::sentences( size_t index ) const {
    vector<Sentence*> v = sentences();
    if ( index < v.size() )
      return v[index];
    else
      throw range_error( "sentences(): index out of range" );
  }

  Sentence *AbstractStructureElement::rsentences( size_t index ) const {
    vector<Sentence*> v = sentences();
    if ( index < v.size() )
      return v[v.size()-1-index];
    else
      throw range_error( "rsentences(): index out of range" );
  }

  Paragraph *AbstractStructureElement::paragraphs( size_t index ) const {
    vector<Paragraph*> v = paragraphs();
    if ( index < v.size() )
      return v[index];
    else
      throw range_error( "paragraphs(): index out of range" );
  }

  Paragraph *AbstractStructureElement::rparagraphs( size_t index ) const {
    vector<Paragraph*> v = paragraphs();
    if ( index < v.size() )
      return v[v.size()-1-index];
    else
      throw range_error( "rparagraphs(): index out of range" );
  }

  Word *AbstractStructureElement::words( size_t index ) const {
    vector<Word*> v = words();
    if ( index < v.size() )
      return v[index];
    else
      throw range_error( "words(): index out of range" );
  }

  Word *AbstractStructureElement::rwords( size_t index ) const {
    vector<Word*> v = words();
    if ( index < v.size() )
      return v[v.size()-1-index];
    else
      throw range_error( "rwords(): index out of range" );
  }

  const Word* AbstractStructureElement::resolveword( const string& id ) const{
    const Word *result = 0;
    for ( size_t i=0; i < data.size(); ++i ){
      result = data[i]->resolveword( id );
      if ( result )
	return result;
    }
    return result;
  }

  vector<Alternative *> AbstractStructureElement::alternatives( ElementType elt,
								const string& st ) const {
    // Return a list of alternatives, either all or only of a specific type, restrained by set
    static set<ElementType> excludeSet;
    if ( excludeSet.empty() ){
      excludeSet.insert( Original_t );
      excludeSet.insert( Suggestion_t );
    }
    vector<Alternative *> alts = select<Alternative>( excludeSet );
    if ( elt == BASE ){
      return alts;
    }
    else {
      vector<Alternative*> res;
      for ( size_t i=0; i < alts.size(); ++i ){
	if ( alts[i]->size() > 0 ) { // child elements?
	  for ( size_t j =0; j < alts[i]->size(); ++j ){
	    if ( alts[i]->index(j)->element_id() == elt &&
		 ( alts[i]->sett().empty() || alts[i]->sett() == st ) ){
	      res.push_back( alts[i] ); // not the child!
	      break; // yield an alternative only once (in case there are multiple matches)
	    }
	  }
	}
      }
      return res;
    }
  }

  KWargs AlignReference::collectAttributes() const {
    KWargs atts;
    atts["id"] = refId;
    atts["type"] = _type;
    if ( !_t.empty() )
      atts["t"] = _t;
    return atts;
  }

  void AlignReference::setAttributes( const KWargs& args ){
    KWargs::const_iterator it = args.find( "id" );
    if ( it != args.end() ){
      refId = it->second;
    }
    it = args.find( "type" );
    if ( it != args.end() ){
      try {
	stringTo<ElementType>( it->second );
      }
      catch (...){
	throw XmlError( "attribute 'type' must be an Element Type" );
      }
      _type = it->second;
    }
    else {
      throw XmlError( "attribute 'type' required for AlignReference" );
    }
    it = args.find( "t" );
    if ( it != args.end() ){
      _t = it->second;
    }
  }


  KWargs Alignment::collectAttributes() const {
    KWargs atts = FoliaElement::collectAttributes();
    if ( !_href.empty() ){
      atts["xlink:href"] = _href;
      if ( !_type.empty() )
	atts["xlink:type"] = _type;
      else
	atts["xlink:type"] = "simple";
    }
    return atts;
  }
  
  void Alignment::setAttributes( const KWargs& args ){
    KWargs::const_iterator it = args.find( "href" );
    if ( it != args.end() ){
      _href = it->second;
    }
    FoliaElement::setAttributes( args );
  }

  void Word::setAttributes( const KWargs& args ){
    KWargs::const_iterator it = args.find( "space" );
    if ( it != args.end() ){
      if ( it->second == "no" ){
	space = false;
      }
    }
    it = args.find( "text" );
    if ( it != args.end() ) {
      settext( it->second );
    }
    FoliaElement::setAttributes( args );
  }

  KWargs Word::collectAttributes() const {
    KWargs atts = FoliaElement::collectAttributes();
    if ( !space ){
      atts["space"] = "no";
    }
    return atts;
  }

  Correction *Word::correct( const string& s ){
    vector<FoliaElement*> nil;
    KWargs args = getArgs( s );
    //  cerr << "word::correct() <== " << this << endl;
    Correction *tmp = AbstractStructureElement::correct( nil, nil, nil, nil, args );
    //  cerr << "word::correct() ==> " << this << endl;
    return tmp;
  }

  Correction *Word::correct( FoliaElement *old,
			     FoliaElement *_new,
			     const KWargs& args ){
    vector<FoliaElement *> nv;
    nv.push_back( _new );
    vector<FoliaElement *> ov;
    ov.push_back( old );
    vector<FoliaElement *> nil;
    //  cerr << "correct() <== " << this;
    Correction *tmp =AbstractStructureElement::correct( ov, nil, nv, nil, args );
    //  cerr << "correct() ==> " << this;
    return tmp;
  }

  FoliaElement *Word::split( FoliaElement *part1, FoliaElement *part2,
			     const string& args ){
    return sentence()->splitWord( this, part1, part2, getArgs(args) );
  }

  FoliaElement *Word::append( FoliaElement *child ) {
    if ( child->isSubClass( TokenAnnotation_t ) ){
      // sanity check, there may be no other child within the same set
      vector<FoliaElement*> v = select( child->element_id(), child->sett() );
      if ( v.empty() ) {
    	// OK!
    	return FoliaElement::append( child );
      }
      delete child;
      throw DuplicateAnnotationError( "Word::append" );
    }
    return FoliaElement::append( child );
  }

  Sentence *Word::sentence( ) const {
    // return the sentence this word is a part of, otherwise return null
    FoliaElement *p = _parent; 
    while( p ){
      if ( p->isinstance( Sentence_t ) )
	return dynamic_cast<Sentence*>(p);
      p = p->parent();
    }
    return 0;
  }

  Paragraph *Word::paragraph( ) const {
    // return the sentence this word is a part of, otherwise return null
    FoliaElement *p = _parent; 
    while( p ){
      if ( p->isinstance( Paragraph_t ) )
	return dynamic_cast<Paragraph*>(p);
      p = p->parent();
    }
    return 0;
  }

  Division *Word::division() const {
    // return the <div> this word is a part of, otherwise return null
    FoliaElement *p = _parent; 
    while( p ){
      if ( p->isinstance( Division_t ) )
	return dynamic_cast<Division*>(p);
      p = p->parent();
    }
    return 0;
  }

  vector<Morpheme *> Word::morphemes( const string& set ) const {
    vector<Morpheme *> result;
    vector<MorphologyLayer*> mv = select<MorphologyLayer>();
    for( size_t i=0; i < mv.size(); ++i ){
      vector<Morpheme*> tmp = mv[i]->select<Morpheme>( set );
      result.insert( result.end(), tmp.begin(), tmp.end() );
    }
    return result;
  }

  Morpheme * Word::morpheme( size_t pos, const string& set ) const {
    vector<Morpheme *> tmp = morphemes( set );
    if ( pos >=0 && pos < tmp.size() )
      return tmp[pos];
    else
      throw range_error( "morpheme() index out of range" );
  }

  Correction *Word::incorrection( ) const {
    // Is the Word part of a correction? If it is, it returns the Correction element, otherwise it returns 0;
    FoliaElement *p = _parent; 
    while( p ){
      if ( p->isinstance( Correction_t ) )
	return dynamic_cast<Correction*>(p);
      else if ( p->isinstance( Sentence_t ) )
	break;
      p = p->parent();
    }
    return 0;
  }

  Word *Word::previous() const{
    Sentence *s = sentence();
    vector<Word*> words = s->words();
    for( size_t i=0; i < words.size(); ++i ){
      if ( words[i] == this ){
	if ( i > 0 )
	  return words[i-1];
	else 
	  return 0;
	break;	
      }
    }
    return 0;
  }
  
  Word *Word::next() const{
    Sentence *s = sentence();
    vector<Word*> words = s->words();
    for( size_t i=0; i < words.size(); ++i ){
      if ( words[i] == this ){
	if ( i+1 < words.size() )
	  return words[i+1];
	else 
	  return 0;
	break;	
      }
    }
    return 0;
  }

  vector<Word*> Word::context( size_t size, 
			       const string& val ) const {
    vector<Word*> result;
    if ( size > 0 ){
      vector<Word*> words = mydoc->words();
      for( size_t i=0; i < words.size(); ++i ){
	if ( words[i] == this ){
	  size_t miss = 0;
	  if ( i < size ){
	    miss = size - i;
	  }
	  for ( size_t index=0; index < miss; ++index ){
	    if ( val.empty() )
	      result.push_back( 0 );
	    else {
	      PlaceHolder *p = new PlaceHolder( "text='" + val + "'" );
	      mydoc->keepForDeletion( p );
	      result.push_back( p );
	    }
	  }
	  for ( size_t index=i-size+miss; index < i + size + 1; ++index ){
	    if ( index < words.size() ){
	      result.push_back( words[index] );
	    }
	    else {
	      if ( val.empty() )
		result.push_back( 0 );
	      else {
		PlaceHolder *p = new PlaceHolder( "text='" + val + "'" );
		mydoc->keepForDeletion( p );
		result.push_back( p );
	      }
	    }
	  }
	  break;
	}
      }
    }
    return result;
  }


  vector<Word*> Word::leftcontext( size_t size, 
				   const string& val ) const {
    //  cerr << "leftcontext : " << size << endl;
    vector<Word*> result;
    if ( size > 0 ){
      vector<Word*> words = mydoc->words();
      for( size_t i=0; i < words.size(); ++i ){
	if ( words[i] == this ){
	  size_t miss = 0;
	  if ( i < size ){
	    miss = size - i;
	  }
	  for ( size_t index=0; index < miss; ++index ){
	    if ( val.empty() )
	      result.push_back( 0 );
	    else {
	      PlaceHolder *p = new PlaceHolder( "text='" + val + "'" );
	      mydoc->keepForDeletion( p );
	      result.push_back( p );
	    }
	  }
	  for ( size_t index=i-size+miss; index < i; ++index ){
	    result.push_back( words[index] );
	  }
	  break;
	}
      }
    }
    return result;
  }

  vector<Word*> Word::rightcontext( size_t size, 
				    const string& val ) const {
    vector<Word*> result;
    //  cerr << "rightcontext : " << size << endl;
    if ( size > 0 ){
      vector<Word*> words = mydoc->words();
      size_t begin;
      size_t end;
      for( size_t i=0; i < words.size(); ++i ){
	if ( words[i] == this ){
	  begin = i + 1;
	  end = begin + size;
	  for ( ; begin < end; ++begin ){
	    if ( begin >= words.size() ){
	      if ( val.empty() )
		result.push_back( 0 );
	      else {
		PlaceHolder *p = new PlaceHolder( "text='" + val + "'" );
		mydoc->keepForDeletion( p );
		result.push_back( p );
	      }
	    }
	    else
	      result.push_back( words[begin] );
	  }
	  break;
	}
      }
    }
    return result;
  }

  const Word* Word::resolveword( const string& id ) const {
    if ( _id == id )
      return this;
    return 0;
  }
  
  vector<FoliaElement*> Word::findspans( ElementType et,
					 const string& st ) const {
    vector<FoliaElement *> result;
    if ( !folia::isSubClass( et , AnnotationLayer_t ) ){
      throw NotImplementedError( "findspans: " + toString( et ) +
				 " is not an AnnotationLayer type" );
    }
    else {
      const FoliaElement *e = parent();
      if ( e ){
	vector<FoliaElement*> v = e->select( et, st, false );
	for ( size_t i=0; i < v.size(); ++i ){
	  for ( size_t k=0; k < v[i]->size(); ++k ){
	    FoliaElement *f = v[i]->index(k);
	    AbstractSpanAnnotation *as = dynamic_cast<AbstractSpanAnnotation*>(f);
	    if ( as ){
	      vector<FoliaElement*> wv = f->wrefs();
	      for ( size_t j=0; j < wv.size(); ++j ){
		if ( wv[j] == this ){
		  result.push_back(f);
		}
	      }
	    }
	  }
	}
      }
    }
    return result;
  }

  FoliaElement* WordReference::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    string id = att["id"];
    if ( id.empty() )
      throw XmlError( "empty id in WordReference" );
    if ( mydoc->debug ) 
      cerr << "Found word reference" << id << endl;
    FoliaElement *res = (*mydoc)[id];
    if ( res ){
      // To DO: check juiste type. Woord, morpheme, dat mag, maar andere?
      res->increfcount();
    }
    else {
      if ( mydoc->debug )
	cerr << "...Unresolvable!" << endl;
      throw XmlError( "Unresolvable id " + id + "in WordReference" );
    }
    delete this;
    return res;
  }

  FoliaElement* AlignReference::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    string val = att["id"];
    if ( val.empty() )
      throw XmlError( "ID required for AlignReference" );
    refId = val;
    if ( mydoc->debug ) 
      cerr << "Found AlignReference ID " << refId << endl;
    val = att["type"];
    if ( val.empty() )
      throw XmlError( "type required for AlignReference" );
    try {
      stringTo<ElementType>( val );
    }
    catch (...){
      throw XmlError( "AlignReference:type must be an Element Type (" 
		      + val + ")" );
    }
    _type = val;
    val = att["t"];
    if ( !val.empty() ){
      _t = val;
    }
    return this;
  }

  FoliaElement *AlignReference::resolve( const Alignment *ref ) const {
    if ( ref->href().empty() )
      return (*mydoc)[refId]; 
    else
      throw NotImplementedError( "resolve for external doc" );
  }

  vector<FoliaElement *> Alignment::resolve() const {
    vector<AlignReference*> v = select<AlignReference>();
    vector<FoliaElement*> result;
    for ( size_t i=0; i < v.size(); ++i ){
      result.push_back( v[i]->resolve( this ) );
    }
    return result;
  }

  void PlaceHolder::setAttributes( const KWargs& args ){
    KWargs::const_iterator it = args.find( "text" );
    if ( it == args.end() ){
      throw ValueError("text attribute is required for " + classname() );
    }
    else if ( args.size() != 1 ){
      throw ValueError("only the text attribute is supported for " + classname() );
    }
    Word::setAttributes( args );
  }

  KWargs TextContent::collectAttributes() const {
    KWargs attribs = FoliaElement::collectAttributes();
    if ( _class == "current" )
      attribs.erase( "class" );
    else if ( _class == "original" && parent()->isinstance( Original_t ) )
      attribs.erase( "class" );    
      
    if ( _offset >= 0 ){
      attribs["offset"] = TiCC::toString( _offset );
    }
    return attribs;
  }

  xmlNode *TextContent::xml( bool, bool ) const {
    xmlNode *e = FoliaElement::xml( false, false );
    xmlAddChild( e, xmlNewText( (const xmlChar*)str().c_str()) );
    return e;
  }

  KWargs Figure::collectAttributes() const {
    KWargs atts = FoliaElement::collectAttributes();
    if ( !_src.empty() ){
      atts["src"] = _src;
    }
    return atts;
  }

  void Figure::setAttributes( const KWargs& kwargsin ){
    KWargs kwargs = kwargsin;
    KWargs::const_iterator it;
    it = kwargs.find( "src" );
    if ( it != kwargs.end() ) {
      _src = it->second;
      kwargs.erase( "src" );
    }
    FoliaElement::setAttributes(kwargs);
  }

  UnicodeString Figure::caption() const {
    vector<FoliaElement *> v = select(Caption_t);
    if ( v.empty() )
      throw NoSuchText("caption");
    else
      return v[0]->text();
  }

  void Description::setAttributes( const KWargs& kwargs ){
    KWargs::const_iterator it;
    it = kwargs.find( "value" );
    if ( it == kwargs.end() ) {
      throw ValueError("value attribute is required for " + classname() );
    }
    _value = it->second;
  }

  xmlNode *Description::xml( bool, bool ) const {
    xmlNode *e = FoliaElement::xml( false, false );
    xmlAddChild( e, xmlNewText( (const xmlChar*)_value.c_str()) );
    return e;
  }

  FoliaElement* Description::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    KWargs::const_iterator it = att.find("value" );
    if ( it == att.end() ){
      att["value"] = XmlContent( node );
    }
    setAttributes( att );
    return this;
  }

  FoliaElement *AbstractSpanAnnotation::append( FoliaElement *child ){
    if ( child->isinstance(PlaceHolder_t) ||
	 ( ( child->isinstance(Word_t) || child->isinstance(Morpheme_t) )
	   && acceptable( WordReference_t ) ) )
      child->increfcount();
    FoliaElement::append( child );
    return child;
  }

  xmlNode *AbstractSpanAnnotation::xml( bool recursive, bool kanon ) const {
    xmlNode *e = FoliaElement::xml( false, false );
    // append Word and Morpheme children as WREFS
    vector<FoliaElement*>::const_iterator it=data.begin();
    while ( it != data.end() ){
      if ( (*it)->element_id() == Word_t ||
	   (*it)->element_id() == Morpheme_t ){
	xmlNode *t = XmlNewNode( foliaNs(), "wref" );
	KWargs attribs;
	attribs["id"] = (*it)->id();
	string txt = (*it)->str();
	if ( !txt.empty() )
	  attribs["t"] = txt;
	addAttributes( t, attribs );
	xmlAddChild( e, t );
      }
      else {
	string at = tagToAtt( *it );
	if ( at.empty() ){
	  // otherwise handled by FoliaElement::xml() above
	  xmlAddChild( e, (*it)->xml( recursive, kanon ) );
	}
      }
      ++it;
    }
    return e;
  }

  FoliaElement *Quote::append( FoliaElement *child ){
    if ( child->isinstance(Sentence_t) )
      child->setAuth( false ); // Sentences under quotes are non-authoritative
    FoliaElement::append( child );
    return child;
  }


  xmlNode *Content::xml( bool recurse, bool ) const {
    xmlNode *e = FoliaElement::xml( recurse, false );
    xmlAddChild( e, xmlNewCDataBlock( 0,
				      (const xmlChar*)value.c_str() ,
				      value.length() ) );
    return e;
  }

  FoliaElement* Content::parseXml( const xmlNode *node ){
    KWargs att = getAttributes( node );
    setAttributes( att );
    xmlNode *p = node->children;
    bool isCdata = false;
    bool isText = false;
    while ( p ){
      if ( p->type == XML_CDATA_SECTION_NODE ){
	if ( isText )
	  throw XmlError( "intermixing text and CDATA in Content node" );
	isCdata = true;
	value += (char*)p->content;
      }
      else if ( p->type == XML_TEXT_NODE ){
	if ( isCdata )
	  throw XmlError( "intermixing text and CDATA in Content node" );
	isText = true;
	value += (char*)p->content;
      }
      else if ( p->type == XML_COMMENT_NODE ){
	string tag = "xml-comment";
	FoliaElement *t = createElement( mydoc, tag );
	if ( t ){
	  t = t->parseXml( p );
	  append( t );
	}
      }
      p = p->next;
    }
    if ( value.empty() )
      throw XmlError( "CDATA or Text expected in Content node" );
    return this;
  }

  UnicodeString Correction::text( const string& cls, bool retaintok ) const {
    if ( cls == "current" ){
      for( size_t i=0; i < data.size(); ++i ){
	//    cerr << "data[" << i << "]=" << data[i] << endl;
	if ( data[i]->isinstance( New_t ) || data[i]->isinstance( Current_t ) )
	  return data[i]->text( cls, retaintok );
      }
    }
    else if ( cls == "original" ){
      for( size_t i=0; i < data.size(); ++i ){
	//    cerr << "data[" << i << "]=" << data[i] << endl;
	if ( data[i]->isinstance( Original_t ) )
	  return data[i]->text( cls, retaintok );
      }
    }
    throw NoSuchText("wrong cls");
  }

  TextContent *Correction::textcontent( const string& cls ) const {
    if ( cls == "current" ){
      for( size_t i=0; i < data.size(); ++i ){
	//    cerr << "data[" << i << "]=" << data[i] << endl;
	if ( data[i]->isinstance( New_t ) || data[i]->isinstance( Current_t ) )
	  return data[i]->textcontent( cls );
      }
    }
    else if ( cls == "original" ){
      for( size_t i=0; i < data.size(); ++i ){
	//    cerr << "data[" << i << "]=" << data[i] << endl;
	if ( data[i]->isinstance( Original_t ) )
	  return data[i]->textcontent( cls );;
      }
    }
    throw NoSuchText("wrong cls");
  }    
  
  bool Correction::hasNew( ) const {
    vector<FoliaElement*> v = select( New_t, false );
    return !v.empty();
  }

  NewElement *Correction::getNew() const {
    vector<NewElement*> v = select<NewElement>( false );
    if ( v.empty() )
      throw NoSuchAnnotation("new");
    else
      return v[0];
  }

  bool Correction::hasOriginal() const { 
    vector<FoliaElement*> v = select( Original_t, false );
    return !v.empty();
  }

  Original *Correction::getOriginal() const { 
    vector<Original*> v = select<Original>( false );
    if ( v.empty() )
      throw NoSuchAnnotation("original");
    else
      return v[0];
  }

  bool Correction::hasCurrent( ) const { 
    vector<FoliaElement*> v = select( Current_t, false );
    return !v.empty();
  }

  Current *Correction::getCurrent( ) const { 
    vector<Current*> v = select<Current>( false );
    if ( v.empty() )
      throw NoSuchAnnotation("current");
    else
      return v[0];
  }

  bool Correction::hasSuggestions( ) const { 
    vector<Suggestion*> v = suggestions();
    return !v.empty();
  }
  
  vector<Suggestion*> Correction::suggestions( ) const {
    return select<Suggestion>( false );
  }

  Suggestion *Correction::suggestions( size_t index ) const { 
    vector<Suggestion*> v = suggestions();
    if ( v.empty() || index >= v.size() )
      throw NoSuchAnnotation("suggestion");
    return v[index];
  }

  Head *Division::head() const {
    if ( data.size() > 0 ||
	 data[0]->element_id() == Head_t ){
      return dynamic_cast<Head*>(data[0]);
    }
    else
      throw runtime_error( "No head" );
    return 0;
  }
  
  string Gap::content() const {
    vector<FoliaElement*> cv = select( Content_t );  
    if ( cv.size() < 1 )
      throw NoSuchAnnotation( "content" );
    else {
      return cv[0]->content();
    }
  }

  Headwords *Dependency::head() const {
    vector<Headwords*> v = select<Headwords>();
    if ( v.size() < 1 )
      throw NoSuchAnnotation( "head" );
    else {
      return v[0];
    }
  } 
    
  DependencyDependent *Dependency::dependent() const {
    vector<DependencyDependent *> v = select<DependencyDependent>();
    if ( v.size() < 1 )
      throw NoSuchAnnotation( "dependent" );
    else {
      return v[0];
    }
  }

  void FoLiA::init(){
    _xmltag="FoLiA";
    _element_id = BASE;
    const ElementType accept[] = { Text_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void DCOI::init(){
    _xmltag="DCOI";
    _element_id = BASE;
    const ElementType accept[] = { Text_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void AbstractStructureElement::init(){
    _element_id = Structure_t;
    _xmltag = "structure";
    _required_attributes = ID;
    _optional_attributes = ALL;
    occurrences_per_set=0;
    TEXTDELIMITER = "\n\n";
  }

  void AbstractTokenAnnotation::init(){
    _element_id = TokenAnnotation_t;
    _xmltag="tokenannotation";
    _required_attributes = CLASS;
    _optional_attributes = ALL;
    occurrences_per_set=1;
  }

  void TextContent::init(){
    _element_id = TextContent_t;
    _xmltag="t";
    _optional_attributes = CLASS|ANNOTATOR|CONFIDENCE|DATETIME;
    _annotation_type = AnnotationType::TEXT;
    occurrences = 0;
    occurrences_per_set=0;
    _offset = -1;
  }

  void Head::init() {
    _element_id = Head_t;
    _xmltag="head";
    const ElementType accept[] = { Structure_t, Description_t, 
				   TextContent_t, Alignment_t, Metric_t,
				   Alternatives_t, TokenAnnotation_t };
    _accepted_data = 
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences=1;
    TEXTDELIMITER = " ";
  }

  void TableHead::init() {
    _element_id = TableHead_t;
    _xmltag="tablehead";
    _required_attributes = NO_ATT;
    const ElementType accept[] = { Row_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TABLE;
  }

  void Table::init() {
    _element_id = Table_t;
    _xmltag="table";
    const ElementType accept[] = { TableHead_t, Row_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TABLE;
  }

  void Cell::init() {
    _element_id = Cell_t;
    _xmltag="cell";
    _required_attributes = NO_ATT;
    const ElementType accept[] = { Structure_t,
				   TokenAnnotation_t,
				   Alignment_t, Metric_t,
				   Alternatives_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TABLE;
    TEXTDELIMITER = " | ";
  }
  
  void Row::init() {
    _element_id = Row_t;
    _xmltag="row";
    _required_attributes = NO_ATT;
    const ElementType accept[] = { Cell_t, };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TABLE;
    TEXTDELIMITER = "\n";
  }

  void LineBreak::init(){
    _xmltag = "br";
    _element_id = LineBreak_t;
    _required_attributes = NO_ATT;
    _annotation_type = AnnotationType::LINEBREAK;
  }

  void WhiteSpace::init(){
    _xmltag = "whitespace";
    _element_id = WhiteSpace_t;
    _required_attributes = NO_ATT;
    _annotation_type = AnnotationType::WHITESPACE;
  }

  void Word::init(){
    _xmltag="w";
    _element_id = Word_t;
    const ElementType accept[] = { TextContent_t, TokenAnnotation_t,
				   Alternative_t,
				   Description_t, 
				   Alignment_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TOKEN;
    TEXTDELIMITER = " ";
    space = true;
  }

  void String::init(){
    _xmltag="str";
    _element_id = Str_t;
    _required_attributes = NO_ATT;
    _optional_attributes = ID|CLASS;
    const ElementType accept[] = { TextContent_t, Correction_t, Lang_t,
				   Description_t, Alignment_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::STRING;
    occurrences = 0;
    occurrences_per_set=0;
  }

  void PlaceHolder::init(){
    _xmltag="placeholder";
    _element_id = PlaceHolder_t;
    _required_attributes = NO_ATT;
  }

  void WordReference::init(){
    _required_attributes = ID;
    _xmltag = "wref";
    _element_id = WordReference_t;
    _auth = false;
  }

  void Alignment::init(){
    _optional_attributes = ALL;
    _xmltag = "alignment";
    _element_id = Alignment_t;
    const ElementType accept[] = { AlignReference_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences_per_set=0;
    _annotation_type = AnnotationType::ALIGNMENT;
    PRINTABLE = false;
  }

  void AlignReference::init(){
    _xmltag = "aref";
    _element_id = AlignReference_t;
  }


  void Gap::init(){
    _xmltag = "gap";
    _element_id = Gap_t;
    _annotation_type = AnnotationType::GAP;
    const ElementType accept[] = { Content_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _optional_attributes = CLASS|ID|ANNOTATOR|CONFIDENCE|N|DATETIME;
  }

  void MetricAnnotation::init(){
    _element_id = Metric_t;
    _xmltag = "metric";
    const ElementType accept[] = { ValueFeature_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _optional_attributes = CLASS|ANNOTATOR|CONFIDENCE;
    _annotation_type = AnnotationType::METRIC;
  }

  void Content::init(){
    _xmltag = "content";
    _element_id = Content_t;
  }

  void Sentence::init(){
    _xmltag="s";
    _element_id = Sentence_t;
    const ElementType accept[] = { Structure_t,
				   TokenAnnotation_t, 
				   TextContent_t, AnnotationLayer_t, 
				   Correction_t,
				   Description_t, Alignment_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::SENTENCE;
  }

  void Division::init(){
    _xmltag="div";
    _element_id = Division_t;
    _required_attributes = ID;
    _optional_attributes = CLASS|N;
    const ElementType accept[] = { Structure_t, Gap_t, 
				   TokenAnnotation_t,
				   Description_t, 
				   TextContent_t,
				   Metric_t, Coreferences_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::DIVISION;
    TEXTDELIMITER = "\n\n\n";
  }

  void Text::init(){
    _xmltag="text";
    _element_id = Text_t;
    const ElementType accept[] = { Gap_t, Division_t, Paragraph_t, Sentence_t, 
				   List_t, Figure_t, Description_t, Event_t,
				   TokenAnnotation_t,
				   TextContent_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _required_attributes = ID;
    TEXTDELIMITER = "\n\n";
  }

  void Event::init(){
    _xmltag="event";
    _element_id = Event_t;
    const ElementType accept[] = { Gap_t, Division_t, Paragraph_t, Sentence_t, 
				   List_t, Figure_t, Description_t, 
				   Feature_t, TextContent_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::EVENT;
    occurrences_per_set=0;
  }

  void TimeSegment::init(){
    _xmltag="timesegment";
    _element_id = TimeSegment_t;
    const ElementType accept[] = { Description_t, Feature_t, Word_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::TIMEDEVENT;
    occurrences_per_set=0;
  }

  void Caption::init(){
    _xmltag="caption";
    _element_id = Caption_t;
    const ElementType accept[] = { Sentence_t, Description_t, 
				   TokenAnnotation_t, TextContent_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences = 1;
  }

  void Label::init(){
    _xmltag="label";
    _element_id = Label_t;
    const ElementType accept[] = { Word_t, Description_t, TextContent_t,
				   TokenAnnotation_t, Alignment_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void ListItem::init(){
    _xmltag="listitem";
    _element_id = ListItem_t;
    const ElementType accept[] = { Structure_t, Description_t, 
				   TokenAnnotation_t,
				   TextContent_t, Alignment_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::LIST;
  }

  void List::init(){
    _xmltag="list";
    _element_id = List_t;
    const ElementType accept[] = { ListItem_t, Description_t,  
				   Caption_t, Event_t, Str_t, Lang_t,
				   TextContent_t, Alignment_t };
    _accepted_data =
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    
    _annotation_type = AnnotationType::LIST;
    TEXTDELIMITER="\n";
  }

  void Figure::init(){
    _xmltag="figure";
    _element_id = Figure_t;
    const ElementType accept[] = { Sentence_t, Description_t, 
				   Caption_t, Str_t, Lang_t, TextContent_t };
    _accepted_data = 
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::FIGURE;
  }

  void Paragraph::init(){
    _xmltag="p";
    _element_id = Paragraph_t;
    const ElementType accept[] = { Structure_t, TextContent_t, 
				   TokenAnnotation_t,
				   Description_t, 
				   Alignment_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::PARAGRAPH;
  }

  void SyntacticUnit::init(){
    _xmltag = "su";
    _element_id = SyntacticUnit_t;
    _required_attributes = NO_ATT;
    _annotation_type = AnnotationType::SYNTAX;
    const ElementType accept[] = { SyntacticUnit_t, Word_t, WordReference_t,
				   Description_t, Feature_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void SemanticRole::init(){
    _xmltag = "semrole";
    _element_id = Semrole_t;
    _required_attributes = CLASS;
    _annotation_type = AnnotationType::SEMROLE;
    const ElementType accept[] = { Word_t, WordReference_t, Lang_t, Headwords_t,
				   Alignment_t, Description_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void Chunk::init(){
    _required_attributes = NO_ATT;
    _xmltag = "chunk";
    _element_id = Chunk_t;
    _annotation_type = AnnotationType::CHUNKING;
    const ElementType accept[] = { Word_t, WordReference_t, Lang_t,
				   Description_t, Feature_t };
    _accepted_data = 
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void Entity::init(){
    _required_attributes = NO_ATT;
    _optional_attributes = ID|CLASS|ANNOTATOR|CONFIDENCE|DATETIME;
    _xmltag = "entity";
    _element_id = Entity_t;
    _annotation_type = AnnotationType::ENTITY;
    const ElementType accept[] = { Word_t, Lang_t, WordReference_t, Morpheme_t,
				   Description_t, Feature_t, Metric_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );;
  }
  
  void AbstractAnnotationLayer::init(){
    _xmltag = "annotationlayer";
    _element_id = AnnotationLayer_t;
    _optional_attributes = SETONLY;
    PRINTABLE=false;
  }

  static ElementType ASlist[] = { SyntacticUnit_t, Chunk_t,
				  Entity_t, Headwords_t,
				  DependencyDependent_t,
				  CoreferenceLink_t,
				  CoreferenceChain_t, Semrole_t,
				  Semroles_t, TimeSegment_t };
  static set<ElementType> asSet( ASlist,
				 ASlist + sizeof( ASlist )/ sizeof( ElementType ) );
  
  vector<AbstractSpanAnnotation*> FoliaElement::selectSpan() const {
    vector<AbstractSpanAnnotation*> res;
    set<ElementType>::const_iterator it = asSet.begin();
    while( it != asSet.end() ){
      vector<FoliaElement*> tmp = select( *it, true );
      for ( size_t i = 0; i < tmp.size(); ++i ){
	res.push_back( dynamic_cast<AbstractSpanAnnotation*>( tmp[i]) );
      }
      ++it;
    }
    return res;
  }
  
  vector<FoliaElement*> AbstractSpanAnnotation::wrefs() const {
    vector<FoliaElement*> res;
    for ( size_t i=0; i < size(); ++ i ){
      ElementType et = data[i]->element_id();
      if ( et == Word_t 
	   || et == WordReference_t 
	   //	   || et == Phoneme_t 
	   || et == Morphology_t ){
	res.push_back( data[i] );
      }
      else {
	AbstractSpanAnnotation *as = dynamic_cast<AbstractSpanAnnotation*>(data[i]);
	if ( as != 0 ){
	  vector<FoliaElement*> sub = as->wrefs();
	  for( size_t j=0; j < sub.size(); ++j ){
	    res.push_back( sub[j] );
	  }
	}
      }
    }
    return res;
  }
  
  FoliaElement *AbstractAnnotationLayer::findspan( const vector<FoliaElement*>& words ) const {
    vector<AbstractSpanAnnotation*> av = selectSpan();
    for ( size_t i=0; i < av.size(); ++i ){
      vector<FoliaElement*> v = av[i]->wrefs();
      if ( v.size() == words.size() ){
	bool ok = true;
	for ( size_t n = 0; n < v.size() && ok ; ++n ){
	  if ( v[n] != words[n] )
	    ok = false;
	}
	if ( ok )
	  return av[i];
      }
    }
    return 0;
  }

  void Alternative::init(){
    _xmltag = "alt";
    _element_id = Alternative_t;
    _required_attributes = NO_ATT;
    _optional_attributes = ALL;
    const ElementType accept[] = { TokenAnnotation_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::ALTERNATIVE;
    PRINTABLE = false;
    AUTH = false;
  }

  void AlternativeLayers::init(){
    _xmltag = "altlayers";
    _element_id = Alternatives_t;
    _optional_attributes = ALL;
    const ElementType accept[] = { AnnotationLayer_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    PRINTABLE = false;
    AUTH = false;
  }

  void AbstractCorrectionChild::init(){
    _optional_attributes = NO_ATT;
    const ElementType accept[] = { TokenAnnotation_t, 
				   Word_t, WordReference_t, Str_t,
				   TextContent_t, Description_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences = 1;
    PRINTABLE=true;
  }
  
  void NewElement::init(){
    _xmltag = "new";
    _element_id = New_t;
  }

  void Current::init(){
    _xmltag = "current";
    _element_id = Current_t;
  }

  void Original::init(){
    _xmltag = "original";
    _element_id = Original_t;
    AUTH = false;
  }

  void Suggestion::init(){
    _xmltag = "suggestion";
    _element_id = Suggestion_t;
    _optional_attributes = ANNOTATOR|CONFIDENCE|DATETIME|N;
    _annotation_type = AnnotationType::SUGGESTION;    
    occurrences=0;
    occurrences_per_set=0;
    AUTH = false;
  }

  void Correction::init(){
    _xmltag = "correction";
    _element_id = Correction_t;
    _required_attributes = NO_ATT;
    _annotation_type = AnnotationType::CORRECTION;
    const ElementType accept[] = { New_t, Original_t, Suggestion_t, Current_t,
				   Description_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences_per_set=0;
  }

  void Description::init(){
    _xmltag = "desc";
    _element_id = Description_t;
    occurrences = 1;
  }

  xmlNode *XmlComment::xml( bool, bool ) const {
    return xmlNewComment( (const xmlChar*)_value.c_str() );
  }
  
  FoliaElement* XmlComment::parseXml( const xmlNode *node ){
    if ( node->content )
      _value = (const char*)node->content;
    return this;
  }

  void XmlComment::init(){
    _xmltag = "xml-comment";
    _element_id = XmlComment_t;
  }

  void Feature::setAttributes( const KWargs& kwargs ){
    //
    // Feature is special. So DON'T call ::setAttributes
    //
    KWargs::const_iterator it = kwargs.find( "subset" );
    if ( it == kwargs.end() ){
      if ( _subset.empty() ) {
	throw ValueError("subset attribute is required for " + classname() );
      }
    }
    else {
      _subset = it->second;
    }
    it = kwargs.find( "cls" );
    if ( it == kwargs.end() )
      it = kwargs.find( "class" );
    if ( it == kwargs.end() ) {
      throw ValueError("class attribute is required for " + classname() );
    }
    _class = it->second;
  }

  KWargs Feature::collectAttributes() const {
    KWargs attribs = FoliaElement::collectAttributes();
    attribs["subset"] = _subset;
    return attribs;
  }

  vector<string> FoliaElement::feats( const std::string& s ) const {
    //    return all classes of the given subset
    vector<string> result;
    for ( size_t i=0; i < data.size(); ++i ){
      if ( data[i]->isSubClass( Feature_t ) &&
	   data[i]->subset() == s ) {
	result.push_back( data[i]->cls() );
      }
    }
    return result;
  }
  
  string FoliaElement::feat( const std::string& s ) const {
    //    return the fist class of the given subset
    for ( size_t i=0; i < data.size(); ++i ){
      if ( data[i]->isSubClass( Feature_t ) &&
	   data[i]->subset() == s ) {
	return data[i]->cls();
      }
    }
    return "";
  }
  
  void Morpheme::init(){
    _element_id = Morpheme_t;
    _xmltag = "morpheme";
    _required_attributes = NO_ATT;
    _optional_attributes = ALL;
    const ElementType accept[] = { Feature_t, FunctionFeature_t, TextContent_t,
				   Metric_t, Alignment_t, TokenAnnotation_t, 
				   Description_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::MORPHOLOGICAL;
  }

  void SyntaxLayer::init(){
    _element_id = SyntaxLayer_t;
    _xmltag = "syntax";
    const ElementType accept[] = { SyntacticUnit_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::SYNTAX;
  }

  void ChunkingLayer::init(){
    _element_id = Chunking_t;
    _xmltag = "chunking";
    const ElementType accept[] = { Chunk_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::CHUNKING;
  }

  void EntitiesLayer::init(){
    _element_id = Entities_t;
    _xmltag = "entities";
    const ElementType accept[] = { Entity_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
   _annotation_type = AnnotationType::ENTITY;
  }

  void TimingLayer::init(){
    _element_id = TimingLayer_t;
    _xmltag = "timing";
    const ElementType accept[] = { TimeSegment_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void MorphologyLayer::init(){
    _element_id = Morphology_t;
    _xmltag = "morphology";
    const ElementType accept[] = { Morpheme_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    occurrences_per_set = 1; // Don't allow duplicates within the same set    
    _annotation_type = AnnotationType::MORPHOLOGICAL;
  }

  void CoreferenceLayer::init(){
    _element_id = Coreferences_t;
    _xmltag = "coreferences";
    const ElementType accept[] = { CoreferenceChain_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::COREFERENCE;
  }

  void CoreferenceLink::init(){
    _element_id = CoreferenceLink_t;
    _xmltag = "coreferencelink";
    _required_attributes = NO_ATT;
    _optional_attributes = ANNOTATOR|N|DATETIME;
    const ElementType accept[] = { Word_t, WordReference_t, Headwords_t, 
				   Description_t, Lang_t,
				   Alignment_t, TimeFeature_t, LevelFeature_t,
				   ModalityFeature_t, Metric_t};
    _accepted_data =
      std::set<ElementType>( accept,
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::COREFERENCE;    
  }

  void CoreferenceChain::init(){
    _element_id = CoreferenceChain_t;
    _xmltag = "coreferencechain";
    _required_attributes = NO_ATT;
    const ElementType accept[] = { CoreferenceLink_t, Description_t,
				   Metric_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::COREFERENCE;    
  }

  void SemanticRolesLayer::init(){
    _element_id = Semroles_t;
    _xmltag = "semroles";
    const ElementType accept[] = { Semrole_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::SEMROLE;
  }

  void DependenciesLayer::init(){
    _element_id = Dependencies_t;
    _xmltag = "dependencies";
    const ElementType accept[] = { Dependency_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    _annotation_type = AnnotationType::DEPENDENCY;
  }

  void Dependency::init(){
    _element_id = Dependency_t;
    _xmltag = "dependency";
    _required_attributes = NO_ATT;
    _annotation_type = AnnotationType::DEPENDENCY;
    const ElementType accept[] = { DependencyDependent_t, Headwords_t, 
				   Feature_t, Description_t, Alignment_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void DependencyDependent::init(){
    _element_id = DependencyDependent_t;
    _xmltag = "dep";
    _required_attributes = NO_ATT;
    _optional_attributes = NO_ATT;
    _annotation_type = AnnotationType::DEPENDENCY;
    const ElementType accept[] = { Word_t, WordReference_t, PlaceHolder_t,
				   Description_t, Feature_t, Alignment_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void Headwords::init(){
    _element_id = Headwords_t;
    _xmltag = "hd";
    _required_attributes = NO_ATT;
    _optional_attributes = NO_ATT;
    const ElementType accept[] = { Word_t, WordReference_t, PlaceHolder_t,
				   Description_t, Feature_t, Metric_t,
				   Alignment_t, Lang_t };
    _accepted_data = 
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void PosAnnotation::init(){
    _xmltag="pos";
    _element_id = Pos_t;
    _annotation_type = AnnotationType::POS;
    const ElementType accept[] = { Feature_t, 
				   Metric_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void LemmaAnnotation::init(){
    _xmltag="lemma";
    _element_id = Lemma_t;
    _annotation_type = AnnotationType::LEMMA;
    const ElementType accept[] = { Feature_t, Metric_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void LangAnnotation::init(){
    _xmltag="lang";
    _element_id = Lang_t;
    _annotation_type = AnnotationType::LANG;
    const ElementType accept[] = { Feature_t, Metric_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void PhonAnnotation::init(){
    _xmltag="phon";
    _element_id = Phon_t;
    _annotation_type = AnnotationType::PHON;
    const ElementType accept[] = { Feature_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void DomainAnnotation::init(){
    _xmltag="domain";
    _element_id = Domain_t;
    _annotation_type = AnnotationType::DOMEIN;
    const ElementType accept[] = { Feature_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void SenseAnnotation::init(){
    _xmltag="sense";
    _element_id = Sense_t;
    _annotation_type = AnnotationType::SENSE;
    const ElementType accept[] = { Feature_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void SubjectivityAnnotation::init(){
    _xmltag="subjectivity";
    _element_id = Subjectivity_t;
    _annotation_type = AnnotationType::SUBJECTIVITY;
    const ElementType accept[] = { Feature_t, Description_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
  }

  void Quote::init(){
    _xmltag="quote";
    _element_id = Quote_t;
    _required_attributes = NO_ATT;
    const ElementType accept[] = { Structure_t, Str_t, Lang_t,
				   TextContent_t, Description_t, Alignment_t };
    _accepted_data =
      std::set<ElementType>( accept, 
			     accept + sizeof(accept)/sizeof(ElementType) );
    TEXTDELIMITER = " ";
  }

  void Feature::init() {
    _xmltag = "feat";
    _element_id = Feature_t;
    occurrences_per_set = 0; // Allow duplicates within the same set
  }

  void BeginDateTimeFeature::init(){
    _xmltag="begindatetime";
    _element_id = BeginDateTimeFeature_t;
    _subset = "begindatetime";
  }

  void EndDateTimeFeature::init(){
    _xmltag="enddatetime";
    _element_id = EndDateTimeFeature_t;
    _subset = "enddatetime";
  }

  void SynsetFeature::init(){
    _xmltag="synset";
    _element_id = SynsetFeature_t;
    _annotation_type = AnnotationType::SENSE;
    _subset = "synset";
  }

  void ActorFeature::init(){
    _xmltag = "actor";
    _element_id = ActorFeature_t;
    _subset = "actor";
  }

  void HeadFeature::init(){
    _xmltag = "headfeature";
    _element_id = HeadFeature_t;
    _subset = "head";
  }

  void ValueFeature::init(){
    _xmltag = "value";
    _element_id = ValueFeature_t;
    _subset = "value";
  }

  void FunctionFeature::init(){
    _xmltag = "function";
    _element_id = FunctionFeature_t;
    _subset = "function";
  }

  void LevelFeature::init(){
    _xmltag = "level";
    _element_id = LevelFeature_t;
    _subset = "level";
  }

  void ModalityFeature::init(){
    _xmltag = "modality";
    _element_id = ModalityFeature_t;
    _subset = "modality";
  }

  void TimeFeature::init(){
    _xmltag = "time";
    _element_id = TimeFeature_t;
    _subset = "time";
  }

  void AbstractSpanAnnotation::init() {
    _required_attributes = NO_ATT;
    _optional_attributes = ALL;
    occurrences_per_set = 0; // Allow duplicates within the same set
  }

  void ErrorDetection::init(){
    _xmltag = "errordetection";
    _element_id = ErrorDetection_t;
    _annotation_type = AnnotationType::ERRORDETECTION;
    occurrences_per_set = 0; // Allow duplicates within the same set
  }

} // namespace folia
