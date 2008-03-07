/***************************************************************************
 *   Copyright (C) 2005 by Jose G de Figueiredo Coutinho                   *
 *   jgfc@doc.ic.ac.uk                                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QRQueryDomain.h>
#include <vector>

using namespace std;

class DomainVisitor: public AstSimpleProcessing {

public:    
    DomainVisitor(set<SgNode *> *domain, QRQueryDomain::TypeDomainExpansion expansionMode) {
	m_root = NULL; m_domain = domain; m_expansionMode = expansionMode;
	for (int i = 0; i < V_SgNumVariants; i++) {
	    m_vectorMask.push_back((VariantT) i);
	}	
    }
    void setRoot(SgNode *node) { m_root = node; }
    
protected:    
    void visit(SgNode *node) {	   
	switch(m_expansionMode) {
	    case QRQueryDomain::immediate_children:
		if (node == m_root) {
		    addTypes(node);
		    addSymbols(node);
  	        }		
   	       if ((node != m_root) && (node->get_parent() != m_root) && 
		   (!isSgMemberFunctionDeclaration (node) || 
		    (m_root != isSgMemberFunctionDeclaration(node)->get_scope ()->get_parent ()))) return;
	        m_domain->insert(node);
		break;
	    case QRQueryDomain::all_children:		
		// add additional nodes that don't get visited from the traversal
                addTypes(node); 
	        addSymbols(node);			 
	        m_domain->insert(node);
	    break;
	}
    }
	
    void addTypes(SgNode *node) {
       list<SgNode *> child_nodes = NodeQuery::querySolverGrammarElementFromVariantVector(isSgNode(node), m_vectorMask);
       for (list<SgNode *>::iterator iter = child_nodes.begin(); 
              iter != child_nodes.end(); iter++) 
       {
	   m_domain->insert(*iter);
       }	
   }
    
    void addSymbols(SgNode *node) {
	if (isSgScopeStatement(node)) {
	   SgSymbolTable *symTable = ((SgScopeStatement *) node)->get_symbol_table();
	   ROSE_ASSERT(symTable);
	   const hash_multimap<SgName,SgSymbol*,hash_Name,eqstr> *symbols = symTable->get_table();
	   if (symbols) {
  	      for (hash_multimap<SgName,SgSymbol*,hash_Name,eqstr>::const_iterator iter = symbols->begin();
	           iter != symbols->end(); iter++)
	      {
	         SgSymbol *symbol = iter->second;
		 if (symbol)
   	            m_domain->insert(symbol);
	      }
	  }
       }			
    }
	
protected:
    SgNode *m_root;    
    set<SgNode *> *m_domain;
    QRQueryDomain::TypeDomainExpansion m_expansionMode;
    NodeQuery::VariantVector m_vectorMask;   
};


QRQueryDomain::QRQueryDomain() {
}
    
unsigned QRQueryDomain::countDomain() {
    return m_domain.size();
}


void QRQueryDomain::expand(std::set<SgNode *> *domain, TypeDomainExpansion expansionMode) {
    m_domain.clear();
        
    if (expansionMode == no_expansion) {
	m_domain = *domain;
    } else {    
        set<SgNode *> &roots = *domain;
        DomainVisitor visitor(&m_domain, expansionMode);
        for (set<SgNode *>::iterator iter = roots.begin(); iter != roots.end(); iter++) {
	   SgNode *root = *iter;
   	   visitor.setRoot(root); visitor.traverse(root,preorder);
        }    
    }
}

void QRQueryDomain::expand(SgNode *domain, TypeDomainExpansion expansionMode) {
    set<SgNode *> nodes;
    nodes.insert(domain);
    expand(&nodes, expansionMode);
}
    

void QRQueryDomain::clear() {
    m_domain.clear();
}
