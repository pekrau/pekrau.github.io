---
categories:
- name: computing
  value: Computing
- name: in-english
  value: In English
- name: vetenskap-science
  value: Vetenskap (science)
date: '2016-04-02'
link: https://kraulis.wordpress.com/2016/04/02/the-mess-in-bioscience-data-handling/
name: the-mess-in-bioscience-data-handling
path: /2016/04/02/the-mess-in-bioscience-data-handling/
tags:
- name: big-data
  value: Big Data
- name: data-storage
  value: data storage
- name: life-science
  value: life science
- name: open-access
  value: Open Access
- name: open-science
  value: Open Science
title: The mess in bioscience data handling
type: post
---
Science is a social activity relying on knowledge sharing, reproducibility, reanalysis and extension of previous work. The movement towards [Open Access](https://en.wikipedia.org/wiki/Open_access) publication and [Open Science](https://en.wikipedia.org/wiki/Open_science) sharing of data and analysis protocols can be seen as a natural development of these ideals. Large data sets are essential to many scientific investigations and are sometimes the product of an investigation. The biosciences have fairly recently started producing large data sets. There are several well-funded international efforts maintaining focused bioscience data sets, such as genomes at [Ensembl](http://www.ensembl.org/), protein sequence data at [UniProt](http://www.uniprot.org/), and many others.

Bioscience researchers are performing more Big Data experiments, but the various infrastructures available at the group, department, university and national levels are unable to cope. The situation for individual research groups is basically a mess. Various *ad hoc* solutions are being implemented, ultimately leading to a patchwork of systems that is becoming increasingly difficult for anyone to navigate. This also makes proper implementation of Open Science extremely hard, if not impossible.



Let's take stock of where we are today. From a situation where each piece of data had to be laboriously obtained one experiment at a time, we are now able to perform experiments that produce large or very large data sets. This is perhaps most obvious in [molecular biology](https://en.wikipedia.org/wiki/Molecular_biology) and [genetics](https://en.wikipedia.org/wiki/Genetics), where new technologies have resulted in the emergence of an entire field of science called [genomics](https://en.wikipedia.org/wiki/Genomics). In the 1980's a high-profile scientific paper could be based on the determination of the DNA sequence of a single gene (e.g. [c-Ha-ras-1](http://www.ncbi.nlm.nih.gov/pubmed/?term=6298635), now called [HRAS](https://en.wikipedia.org/wiki/HRAS)), one of the first cancer-related genes to be sequenced). Research groups spent months getting the complete sequence of maybe 5,000 bases or so for a single gene. Today, a high-end sequencing machine such as the [Illumina HiSeq X](http://www.illumina.com/systems/sequencing.html) can produce 1,800,000,000,000 bases of DNA sequence in less than three days.

Obviously, this creates new problems. One issue is that computing technology has not kept up with the break-neck speed of improvement. Sequencing technology has [improved at a rate substantially faster](https://www.genome.gov/sequencingcosts/) than [Moore's law](https://en.wikipedia.org/wiki/Moore%27s_law). It is increasingly difficult to match scientific demands for new sequencing capacity with the infrastructure required to handle data storage and computation. In some cases, funding agencies appear to find new sequencing capacity to be more "sexy" than data storage, allocating grants for the one but not the other. In other cases, funding is allocated to both, but various bureaucratic convolutions during procurement may delay in delivery of systems, which leads to forced inactivity and frustration.

A somewhat embarrassing example at the national level is [SweStore](http://snicdocs.nsc.liu.se/wiki/SweStore) (also [here](http://www.snic.vr.se/projects/swestore)). It is a Swedish national infrastructure that is described thus:

> The aim of the nationally accessible storage is to build a robust, flexible and expandable system that can be used in most cases where access to large scale storage is needed. To the user, it appear as a single large system, while it is desirable that some parts of the system are distributed to benefit from the advantages of, among other things, locality and cache effects. The system is intended as a versatile long-term storage system.

Quote from the [SNIC page for SweStore](http://www.snic.vr.se/projects/swestore).

Excellent! This sounds like what we need. Let's ignore the troubling fact that SweStore is classed as a "project" by SNIC, which implies that it will have an end. And never mind that the application process, the interface, and so on, has the usual user-hostile design one has come to associate with academic computing. A research group can always force an unfortunate Ph.D. student to struggle with those things. At least it appears that SweStore provides badly needed data storage resources.

But wait! In the page of the first link it says (last checked 2 April 2016):

> Swestore currently has a lack of resources. New projects will not be allocated until Spring 2016. In case you have any questions, please contact the SNIC office (office@snic.se).

**Not so excellent. The fact that this situation has even been permitted to occur shows that something in the Swedish science system is seriously broken. How can Swedish science be serious about Big Data if this is allowed to happen?**

However, it's about more than just coping with the sheer amount of data.

Scientists are asking why data sets and analysis protocols of other scientists should be so hard to find and retrieve. Data can be buried as essentially unusable PDF files attached to research publications. Data sets are sometimes available only upon request, or at web sites which disappear after some indeterminate time. Tax payers are wondering why they finance research which becomes available only through publications behind paywalls. The general public as well as the scientific community has reason to ask what is being done to combat the problem of shoddy and/or outright fraudulent science, a problem whose magnitude is larger than has been generally recognized.

**It is clear that the response to these challenges should involve he application of the ideal of openness inherent to science. Scientific data, protocols as well as publication must be made available for general inspection and use. This is the main reason why the movement towards [Open Access](https://en.wikipedia.org/wiki/Open_access) publication and [Open Science](https://en.wikipedia.org/wiki/Open_science) sharing of data and analysis protocols is gaining traction.**

The immediate problem faced by bioscience is one of finding storage space for the data it produces. My experience at the [National Genomics Infrastructure in Sweden](https://www.scilifelab.se/platforms/ngi/) is that this problem is consuming a fair amount of the attention of the producers as well as end-users of data. We use facilities at [UPPMAX](http://www.uppmax.uu.se/) which have been partly financed and designed specifically for our needs. The daily struggle to free up storage space to handle new data produced by the NGI sequencers fills a significant part of each working day. Some end-user scientists are experienced in computing and know how to deal with the data sets. Others are novices, and find it very hard to navigate the complexities of data handling. Although it is hard to gauge the problem, it is possible that at least some researchers decide to work with their data on local resources, such as a collection of desk-side terabyte disk drives. Anyone with any experience of how things usually are organized in individual research groups knows that this is a recipe for disaster.

To conclude: The situation for data storage for bioscience in Sweden is a mess. This causes real problems for biomedical research, and is an impediment for the development of Open Science.

What is to be done? In the near future, I intend to write about my vision for a useful and viable storage system in Sweden. Stay tuned.

