'''Basic URL creation for cross-references to external web resources.
The implemented web resources are mostly in bioinformatics,
but the design is applicable in general.

Synthesize URLs for access and/or download from the search string
('xkey') of the specified external database ('xdb').

Note: UniProt 'beta.uniprot.org' address is used; change when not beta anymore.

Example usage:

    import xrefresolver
    a = xrefresolver.a_href('PDB', '1CRQ')
    print a
    a = xrefresolver.Ensembl('ENSG00000174775').a_href()
    print a
    url = xrefresolver.UniProt('P01112').url()
    print url
    xmldata = xrefresolver.UniProt('P01112').get_xml()
    print len(xmldata)
    print xmldata[:80] + '...'

This is the output:

<a href="http://www.rcsb.org/pdb/explore.do?structureId=1CRQ">PDB:1CRQ</a>
<a href="http://www.ensembl.org/Homo_sapiens/geneview?gene=ENSG00000174775">Ensembl:ENSG00000174775</a>
http://beta.uniprot.org/uniprot/P01112
48252
<?xml version='1.0' encoding='UTF-8'?>
<uniprot xmlns="http://uniprot.org/unipro...

------------------------------------------------------------
Version 1.1
 
2005-10-16
2007-09-20  checked templates, use urllib2, error handling for get_xml
 
Copyright (C) 2005-2007 Per Kraulis
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''

__version__ = '1.1'

import urllib2


def a_href(xdb, xkey, title=None):
    """Return the XHTML for the xref given by xdb and xky.
    Return an 'a' href element if possible, else a string."""
    resolver = xref_resolver(xdb)
    if resolver is None:
        items = [xdb, xkey]
        if title: items.append(title)
        return escape(':'.join(items))
    else:
        if issubclass(resolver, XkeyTemplate):
            return str(resolver(xkey, title))
        else:
            return str(resolver(xdb, xkey, title))


class XrefResolver(object):
    "Base resolver class. To be elaborated, rather than instantiated directly."

    def __init__(self, xdb, xkey, title=None):
        self.xdb = xdb
        self.xkey = xkey
        self.title = title

    def __str__(self):
        "Same as method 'a_href'."
        return self.a_href()

    def a_href(self, full=True):
        "Return the XHTML 'a' href element (HTML 'A') for this xref."
        if self.title:
            if full:
                title = "%s:%s %s" % (self.xdb, self.xkey, self.title)
            else:
                title = self.title
        else:
            title = "%s:%s" % (self.xdb, self.xkey)
        url = self.url()
        if url is None:
            return title
        else:
            return '<a href="%s">%s</a>' % (url, escape(title))

    def url(self):
        "Return the resolved URL for the xref."
        raise NotImplementedError

    def urn(self):
        "Return the URN for the xref."
        return "urn:%s:%s" % (self.xdb, self.xkey)


class url(XrefResolver):
    "An xref that is simply a common URL address."
    def url(self):
        "Return the URL as is."
        return self.xkey


class XkeyTemplate(XrefResolver):
    """Base resolver class for the simple case where the
    xkey can be inserted directly into the URL template.
    The name of the class becomes the value of the 'xdb' member.
    Different formats may be specified in the dictionary using
    the format identifier as key and the corresponding template
    as value."""

    template = None
    format_templates = dict()

    def __init__(self, xkey, title=None):
        super(XkeyTemplate, self).__init__(self.__class__.__name__,
                                           xkey, title)

    def url(self, format=None):
        """Return the resolved URL for the xref.
        If the format is given, then the resource must be able to
        provide it, or a ValueError is raised.
        By default, a HTML resource is assumed.
        """
        if format is None:
            template = self.template
        else:
            try:
                template = self.format_templates[format]
            except KeyError:
                raise ValueError("%s: no template for format '%s'"
                                 % (self.urn(), format))
        return template % self.xkey

    def get(self, format=None):
        """Get the data for the resource. Raise IOError if not found.
        This is a basic implementation that may have to be redefined
        for s specific Xdb depending on how it behaves."""
        try:
            return urllib2.urlopen(self.url(format=format)).read()
        except urllib2.HTTPError, msg:
            raise IOError(msg)

    def get_xml(self):
        """Get the XML data for the resource. Raise IOError if not found.
        This is a basic implementation that may have to be redefined
        for s specific Xdb depending on how it behaves."""
        return self.get(format='XML')


class Wikipedia(XkeyTemplate):
    "The English Wikipedia resource."
    template='http://en.wikipedia.org/wiki/%s'

class DOI(XkeyTemplate):
    "Digital Object Identifier lookup and forwarding service."
    template='http://dx.doi.org/%s'

class PubMed(XkeyTemplate):
    "PubMed life sciences literature lookup service."
    template='http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=PubMed&dopt=Abstract&list_uids=%s'
    format_templates = dict(XML="http://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?retmode=xml&db=PubMed&id=%s")

    def get_xml(self):
        data = super(PubMed, self).get_xml()
        if '<ERROR>' in data:
            raise IOError('Resource not found.')
        return data


class PubMedSearch(XkeyTemplate):
    "PubMed life sciences literature search service."
    template='http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Search&db=PubMed&term=%s'

class PubMedBook(XkeyTemplate):
    "PubMed life sciences literature on-line textbook service."
    template='http://www.ncbi.nlm.nih.gov/books/bv.fcgi?rid=%s'

class NCBITaxonomy(XkeyTemplate):
    "NCBI Taxonomy lookup service."
    template='http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?id=%s&mode=Info'

class PubChem(XkeyTemplate):
    "PubMed chemical compound lookup service."
    template='http://pubchem.ncbi.nlm.nih.gov/summary/summary.cgi?cid=%s'

class OMIM(XkeyTemplate):
    "Online Mendelian Inheritance in Man, genetic disorders lookup service."
    template='http://www.ncbi.nlm.nih.gov/entrez/dispomim.cgi?id=%s'

class EntrezGene(XkeyTemplate):
    "NCBI Entrez Gene database of genes, lookup service."
    template='http://www.ncbi.nlm.nih.gov/sites/entrez?Db=gene&Cmd=ShowDetailView&TermToSearch=%s'

class GenBank(XkeyTemplate):
    "NCBI nucleotide sequence database lookup service."
    "GenBank using GI (without 'GI:' part) or accession code as xkey."
    template='http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi?db=nucleotide&id=%s'

class HGNC(XkeyTemplate):
    """HUGO Gene Nomenclature Committee, gene name lookup service.
    The ID is the xkey. It is unclear how to use the approved symbol."""
    template='http://www.genenames.org/data/hgnc_data.php?hgnc_id=%s'


class Ensembl(XkeyTemplate):
    """EMBL & Sanger eukaryotic genome database lookup service.
    Determine the Ensembl address template by looking at the xkey prefix.
    This implementation cannot handle XML format (if there were any...)."""

    template = 'http://www.ensembl.org/%s/%s'
    tags = {'ENSG': ('Homo_sapiens', 'geneview?gene=%s'),
            'ENST': ('Homo_sapiens', 'transview?transcript=%s'),
            'ENSP': ('Homo_sapiens', 'protview?peptide=%s'),
            'ENSMUSG': ('Mus_musculus', 'geneview?gene=%s'),
            'ENSMUST': ('Mus_musculus', 'transview?transcript=%s'),
            'ENSMUSP': ('Mus_musculus', 'protview?peptide=%s'),
            'ENSRNOG': ('Rattus_norvegicus', 'geneview?gene=%s'),
            'ENSRNOT': ('Rattus_norvegicus', 'transview?transcript=%s'),
            'ENSRNOP': ('Rattus_norvegicus', 'protview?peptide=%s')}

    def url(self, format=None):
        """Return the resolved URL for the xref.
        The format argument is currently ignored."""
        prefix = ''
        for c in self.xkey:
            if not c.isalpha(): break
            prefix += c
        try:
            template = self.template % self.tags[prefix]
        except KeyError:
            return None
        return template % self.xkey

class UniProt(XkeyTemplate):
    "UniProt, protein sequence database lookup service."
    template='http://beta.uniprot.org/uniprot/%s'
    format_templates = dict(XML=template+'.xml')

class PDB(XkeyTemplate):
    "PDB at RSCB, biomolecular 3D structure data lookup service."
    template='http://www.rcsb.org/pdb/explore.do?structureId=%s'
    format_templates = dict(XML='http://www.rcsb.org/pdb/download/downloadFile.do?fileFormat=xml&compression=NO&structureId=%s')

    def get_xml(self):
        data = super(PDB, self).get_xml()
        if 'The file you requested does not exist.' in data:
            raise IOError('Resource not found.')
        return data


class WormBase(XkeyTemplate):
    "C. elegansDrosophila genetics database lookup service."
    template='http://www.wormbase.org/db/gene/gene?name=%s;class=Gene'
    format_templates = dict(XML='http://www.wormbase.org/db/misc/xml?name=%s;class=Gene')

class FlyBase(XkeyTemplate):
    "Drosophila genetics database lookup service."
    template='http://www.flybase.net/reports/%s.html'

class EC(XkeyTemplate):
    """The Enzyme Commission classification system for enzymes.
    The address for the Enzyme Commission site is derived from
    the EC number in an absolutely brain-dead way..."""

    template = 'http://www.chem.qmul.ac.uk/iubmb/enzyme/EC%s'

    def url(self, format=None):
        """Return the resolved URL for the xref.
        The format argument is currently ignored."""
        parts = self.xkey.split('.')
        result = self.template % '/'.join(parts)
        if len(parts) == 4:
            result += '.html'
        return result

class BRENDA(XkeyTemplate):
    "BRENDA enzyme database lookup service."
    template = 'http://www.brenda-enzymes.org/php/result_flat.php4?ecno=%s'

class Reactome(XkeyTemplate):
    "Reactome molecular events and pathways database lookup service."
    template='http://www.reactome.org/cgi-bin/link?SOURCE=Reactome&ID=%s'

class SignalingGateway(XkeyTemplate):
    "Signaling Gateway, molecular signal transduction database lookup service."
    template='http://www.signaling-gateway.org/molecule/query?afcsid=%s'

class KeggGene(XkeyTemplate):
    """Kyoto Encyclopedia of Genes and Genomes, gene ookup service.
    The xkey is in the form 'hsa:3265'."""
    template = 'http://www.genome.ad.jp/dbget-bin/www_bget?%s'

class KeggPathway(XkeyTemplate):
    """Kyoto Encyclopedia of Genes and Genomes, pathway lookup service.
    The xkey is in the form 'hsa04010'."""
    template='http://www.genome.ad.jp/dbget-bin/show_pathway?%s'


# Set up list and lookup for all XrefResolver subclasses

xdbs = []
xdb_classes = {}

def xref_resolver(xdb):
    "Return the resolver class for the xdb."
    return xdb_classes.get(xdb.lower())

def add_resolver_class(resolver_class, name=None):
    "Add an xref resolver class to the lookup."
    if type(resolver_class) != type(XrefResolver): return
    if not issubclass(resolver_class, XrefResolver): return
    if resolver_class in [XrefResolver, XkeyTemplate]: return
    if name is None:
        name = resolver_class.__name__
    xdb_classes[name.lower()] = resolver_class
    xdbs.append(name)

for resolver_class in locals().values():
    add_resolver_class(resolver_class)
xdbs.sort()

def escape(s):
    "Helper function to replace special characters with their HTML entities."
    s = s.replace("&", "&amp;") # Must be done first!
    s = s.replace("<", "&lt;")
    s = s.replace(">", "&gt;")
    s = s.replace('"', "&quot;")
    return s

def test_xml():
    xrefs = [UniProt('P01112', 'RASH_HUMAN'),UniProt('P01112012345'),
             PubMed('8142349'),PubMed('1234568142349'),
             PDB('1CRQ'), PDB('0CRQ'),
             WormBase('WBGene00004310')]
    for xref in xrefs:
        print '-' * 10, xref.urn(), '-' * 10
        try:
            data = xref.get_xml()
            print "%i bytes" % len(data)
            print data
        except IOError, msg:
            print 'IOError:', msg

def test_get(xref):
    print '-' * 10, xref.urn()
    print '-' * 10, xref.url()
    try:
        print xref.get()
    except IOError, msg:
        print 'IOError:', msg

def test_usage():
    a = a_href('PDB', '1CRQ')
    print a
    a = Ensembl('ENSG00000174775').a_href()
    print a
    url = UniProt('P01112').url()
    print url
    xmldata = UniProt('P01112').get_xml()
    print len(xmldata)
    print xmldata[:80] + '...'


if __name__ == '__main__':
    test_usage()
##     test_xml()
##     test_get(EC('3.6.5.2'))
##     test_get(Ensembl('ENSG00000174775'))
##     test_get(GenBank('NP_005334'))
##     test_get(GenBank('4885425'))
##     test_get(SignalingGateway('A000003'))
##     test_get(Reactome('REACT_4795'))
##     test_get(DOI('10.1038/356448a0'))
##     test_get(FlyBase('FBgn0003205'))
##     test_get(PDB('1CRQ'))
##     test_get(EntrezGene('698830'))
##     test_get(OMIM('190020'))
##     test_get(PubChem('6830'))
##     test_get(NCBITaxonomy('9606'))
##     test_get(PubMedSearch('ras'))
##     test_get(PubMedBook('stryer.section.2093#2104'))
##     test_get(Wikipedia('Ras'))
##     test_get(PubMed('8142349'))
