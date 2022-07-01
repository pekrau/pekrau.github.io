---
categories:
- name: computing
  value: Computing
- name: in-english
  value: In English
- name: vetenskap-science
  value: Vetenskap (science)
date: '2016-06-21'
link: https://kraulis.wordpress.com/2016/06/21/a-researchers-view-of-data-handling-for-life-science/
name: a-researchers-view-of-data-handling-for-life-science
path: /2016/06/21/a-researchers-view-of-data-handling-for-life-science/
tags:
- name: big-data
  value: Big Data
- name: data-storage
  value: data storage
- name: open-access
  value: Open Access
- name: open-science
  value: Open Science
title: A researcher's view of data handling for life science
type: post
---
Given the current mess of data handling in life science (or bioscience, as it is also called) which I described in [a previous article](/2016/04/02/the-mess-in-bioscience-data-handling/), what should be done? Let us begin with a few words from one of the gurus:

> You have to start with the customer experience and work backwards to the technology.

Steve Jobs, quoted [here](http://www.imore.com/steve-jobs-you-have-start-customer-experience-and-work-backwards-technology).

We should start by defining what the needs are. What does the scientist, the research group, want in terms of data storage and handling? What do they need in order to pursue successful life science? What other goals for data storage in Swedish science are there? How can we promote approaches to data handling that facilitate Open Science?

This text is not a final text or treatise. It's a snapshot of my thinking on the subject. Serious policy and design specifications must of course be crafted through debate and input from various experts. There is now an initiative at the [Science for Life Laboratory](https://www.scilifelab.se/), where I work, to discuss these issues. I have written this as a starting point for the discussion, in the hope that it may be useful for SciLifeLab and for others.



Let me begin by describing some of the needs that should be met, and some goals we should have. This is based on my experience at SciLifeLab and on discussions with other researchers. Only by having first discussed the issues and the goals can we then proceed to questions about resources and technology. **The engineer in me says: Specifications first, then design and implementation.** Too often, the discussion revolves around concrete technical solutions before the needs have been properly analyzed. The danger is that we create *ad hoc* solutions that become fragmented, are badly designed, difficult to maintain, and very hard for the user to understand.

Of course, this does not mean that we have to specify the ideal system in complete detail. That is just impossible. Instead, we have to figure out which issues are simpler and more fundamental, and which are more complex. We can then organize the work into providing the basics first, add on increasingly complex layers as we progress. We may discover at some level that the some of the basic building blocks are not good enough. Then we need to revisit those before solving the higher-level problem. Hierarchy is essential, but cannot be just top-down.

### Rosalind's day in the lab

Let's follow an imaginary life science researcher called Rosalind during a day in the lab. What challenges does she face?

Rosalind picks up a package delivered by courier containing a 1 TB hard disk drive with the RNA-Seq data for the samples she sent to a sequencing service. She connects the disk to her laptop, and starts the script that checks the integrity of the data. It takes a couple of hours. She doesn't have enough space in her laptop to copy the data over, so lets it stay on the delivery disk.

Rosalind needs to compare the new data with some old pilot experiments run by her former Ph.D. student James who is now a postdoc in another lab. She searches her laptop for the notebook files copied from James's machine before he left, and after some digging among the folders, finds the file name in an entry. Browsing the entry, she finds the name of the hard drive where it is supposed to be. After some fruitless digging around in the cabinet in her office, she goes to the cupboard James had for his training gear and other stuff, and finally finds the disk there, with another four disks. She had told James to clean out that cupboard, but that had never happened. That's so like James! He never did bother with the boring stuff. Thankfully, the disks do seem to have helpful labels written on them.

Now, how does she know that the file she has found on the disk is the one used for the final analysis  in the paper that's in press? She knows that James had to rerun the pipeline several times to iron out various minor issues. But did James rename the old files? She cannot find any MD5 hash values in James' notebook, and she knows there are none in the supplementary material in the paper. The journal didn't accept the files themselves as supplementary material, as she had hoped they would. She has to take it on trust that the files she found on James' disk are the final ones.

Now she has to find space somewhere on the server to run the analysis. She manages to free up enough space to transfer the data onto one of the fast disks on the server, but only by deleting some files she thinks are intermediate steps from James' previous work. She's uncomfortable doing it, but she doesn't have enough grant money to buy more fast disk this year.

Coming back the next day, she has a set of result files. Now, how to record the fact that these files were derived from the original RNA-Seq data? She tacks on one more suffix on the file names, to show which step of the analysis she is at. This file name is the handle she will use in her notes.

While her other scripts are chugging along, she starts to think about how to backup and safely store the original RNA-Seq data. She know that the sequencing service does not guarantee anything. The university has no storage facility for this kind of data. Using [box.com](https://www.box.com/), which the university has said is the official solution, is out of the question. Storing 600 GB there? Try uploading that, good luck! It seems she will have to do it the usual, ugly way. Go to a computer store and buy a couple of 1TB disk drives, one to keep at home, and one for the lab. That ought to do it.

### So, what is needed?

Here is an attempt at distilling the unmet needs experienced by Rosalind. The ideal storage system should satisfy the following requirements:

1. It must provide enough storage with good I/O performance for analysis to be performed.
2. Allow computation and analysis, i.e. be located so that CPU resources can access the data efficiently.
3. Allow stowing away data that is not going to be analysed just right now.
4. Allow archiving data that is to be stored indefinitely.
5. Data that has been stowed or archived must be easily (ideally transparently) moved back to high-performance storage, for renewed analysis.
6. Be safe, i.e. not become corrupted or changed inadvertently.
7. Be secure, i.e. not be accessed or modified by others before time, or due to privacy concerns.
8. Allow collaboration with involved partners, which may be located globally.
9. Allow the data to be tagged or otherwise enriched by metadata, during analysis, and for later publication.
10. Allow files to be frozen, i.e. protected from being overwritten.
11. All files should have an MD5 hash value associated with them. This can be used as a file reference, as well as an integrity mark.

This list is neither complete, nor well structured. Please feel free to comment below.

### The basics of a solution

Here are some ideas about the basic design of a storage system that I believe would help to meet the needs discussed above. This is obviously rather short on detail. But I hope it can help get the discussion going.

#### Cloud, and yet localized

The storage services such as [Amazon S3](https://aws.amazon.com/s3/), [box.com](https://www.box.com/) and [DropBox](https://www.dropbox.com/) should be inspirational models. Their block-based storage design is appropriate for files that are processed as a whole. These services are not appropriate for databases where items or records in a large data set are accessed in a non-deterministic manner.

Many applications in the life sciences do process files as entire blocks of data. The main problem for life science researchers is that analysis is very I/O intensive, meaning that the CPUs must have efficient access to the disks where the data is located. If, for instance. Amazon S3 is used, then computation must use Amazon's own compute service EC2, otherwise the operation becomes both too expensive and too inefficient. This is not even possible for box.com and DropBox, there are no fast CPU resources connected to those.

So for academic life science, we would like to have a cloud system that allows files to be transparently relocated to the compute center where the analysis is to be done. Ideally, this would be done transparently, when the scripts accessing the data are started, but a simple web interface to control where data files are to be moved would be a reasonable solution.

#### Different levels of storage

Storage is expensive. Some technical solutions are cheaper than others, but are also slower. This is likely to be the case for the foreseeable future. So we need a system where only the data that we really need to process (right now!) is located on the storage having the best performance, since this is the most expensive.

We also need a system for archiving. Swedish universities are by law required to store data for at least 10 years, but of course science needs archiving for its own purposes, and for longer than 10 years. This kind of storage basically just has to be durable. It would be OK if it is very slow to access. I think most researchers can live with a solution where archived data takes on the order of a few days to bring back to some faster medium.

Considering the current setup of computing centers in Sweden, it might be reasonable to have an additional layer of half-fast storage in between the other two extremes. For instance, some data sets are required multiple times as reference for some analysis, and it might be useful to have that data located on a cheaper system than the fastest storage. But this is a question that the technical experts can decide. Is the additional complexity worth the savings?

#### Allocation of storage: Quotas? Grants? Payment?

The trickiest question of all is really how to regulate the amount of storage space a researcher is allocated. Experience shows that a researcher will fill up her allocated storage space, no matter how large. To provide researchers with as much storage as they want is of course not sustainable, or even possible. So some quota system is required.

Quotas can work without involving payments. Quotas can be allocated based on many different criteria, such as the nature of the research, excellence judged in some way, or how efficient the researcher has shown herself to be in using computing resources.

Quotas are, however, also a major pain in the neck. If the researcher needs to do some unanticipated analysis, and that is often what science is all about, then she may have to apply for a larger quota than usual. How is that going to be handled in a fair and efficient way?

The idea described above with a three-layer (or two) storage system could help. By setting fairly strict quotas on the most expensive storage, but allowing very large quotas on the slow archival storage, the responsibility for shuffling data appropriately will be pushed to the researcher. That's where it must be. No-one else can decide which data should be moved from one system to the other. Time limits for having a data set on fast storage, for instance, are not going to work. It is just not possible to say how long it will take to develop a novel analysis method, for instance.

An additional benefit of a layered system controlled by the researcher is that a payment system can be added on top. If a researcher wants more fast storage than she gets in the quota system, she can pay for it herself, and get that amount added to her quota.

There have been suggestions that universities should pay for the storage used by the researchers. This may be reasonable (I do not know), but it **does not** solve the issue of making researchers behave responsibly using storage space. University financing is an orthogonal issue to the problem of how to design a sustainable system for researchers.

#### Web front end

If the layered system above is realized, then a web interface would be the natural way for a researcher to control which data sets should be shuffled between which modes of storage, and which compute centers. By making transfer requests in the web interface, and following their progress, the scientist can be spared the usual horror of using scp or rsync between machines.

A web front end would also allow researchers to publish specific data sets for papers or manuscripts,  providing stable URLs such as [DOIs](https://www.doi.org/). Tags should be used to mark up each data set, to allow for searches. A web interface is a requirement for Open Science: A data set that does not appear on the Web, in one way or another, is not Open.

**So, there it is. Now, discuss!**

