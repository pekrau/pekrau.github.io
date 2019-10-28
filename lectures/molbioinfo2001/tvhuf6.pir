ENTRY           TVHUF6  #type complete
TITLE           protein kinase raf-1 (EC 2.7.1.-) - human
ALTERNATE_NAMES kinase-related transforming protein raf-1; raf-1
                proto-oncogene protein-serine/threonine kinase
CONTAINS        protein kinase (EC 2.7.1.37)
ORGANISM        #formal_name Homo sapiens #common_name man
DATE            04-Dec-1986 #sequence_revision 04-Dec-1986 #text_change
                11-Jun-1999
ACCESSIONS      A00637; I57580
REFERENCE       A00637
   #authors     Bonner, T.I.; Oppermann, H.; Seeburg, P.; Kerby, S.B.;
                Gunnell, M.A.; Young, A.C.; Rapp, U.R.
   #journal     Nucleic Acids Res. (1986) 14:1009-1015
   #title       The complete coding sequence of the human raf oncogene and
                the corresponding structure of the c-raf-1 gene.
   #cross-references MUID:86120351
   #accession   A00637
      ##molecule_type mRNA
      ##residues 1-648 ##label BON1
      ##cross-references GB:X03484; NID:g35841; PIDN:CAA27204.1;
                PID:g35842
REFERENCE       I57580
   #authors     Bonner, T.I.; Kerby, S.B.; Sutrave, P.; Gunnell, M.A.;
                Mark, G.; Rapp, U.R.
   #journal     Mol. Cell. Biol. (1985) 5:1400-1407
   #title       Structure and biological activity of human homologs of the
                raf/mil oncogene.
   #cross-references MUID:85295973
   #accession   I57580
      ##status translated from GB/EMBL/DDBJ
      ##molecule_type DNA
      ##residues 228-239,'L',241-541,'I',543-648 ##label BON2
      ##cross-references GB:L00212; NID:g190837; PIDN:AAA60247.1;
                PID:g496091
REFERENCE       A43089
   #authors     Morrison, D.K.; Heidecker, G.; Rapp, U.R.; Copeland, T.D.
   #journal     J. Biol. Chem. (1993) 268:17309-17316
   #title       Identification of the major phosphorylation sites of the
                Raf-1 kinase.
   #cross-references MUID:93352516
   #contents    annotation; phosphorylation sites
   #note        expression is ubiquitous in mammalian tissues that have
                been studied
COMMENT         After phosphorylation and activation by protein kinase C
                and other kinases, this kinase is responsible for
                activating MAP kinase kinase (see PIR:A45100 and
                PIR:A46723).
GENETICS
   #gene        GDB:RAF1
      ##cross-references GDB:119546; OMIM:164760
   #map_position 3p25-3p25
   #introns     278/3; 288/1; 330/3; 370/1; 398/2; 457/2; 473/1; 512/3;
                556/3; 601/3
   #note        the list of introns is incomplete
FUNCTION
   #description catalyzes the formation of specific
                peptidyl-threonine-phosphate and peptidyl-serine-phosphate
                residues in MAP kinase kinase using ATP
   #pathway     MAP kinase cascade
CLASSIFICATION  #superfamily protein kinase A-raf; protein kinase C
                zinc-binding repeat homology; protein kinase homology
KEYWORDS        ATP; autophosphorylation; phosphoprotein;
                phosphotransferase; proto-oncogene; serine/
                threonine-specific protein kinase; signal transduction;
                transforming protein; zinc
FEATURE
   139-184               #domain protein kinase C zinc-binding repeat
                         homology #label KZN\
   347-613               #domain protein kinase homology #label KIN\
   355-363               #region protein kinase ATP-binding motif\
   43,621                #binding_site phosphate (Ser) (covalent) #status
                         experimental\
   139,165,168,184       #binding_site zinc (His, Cys, Cys, Cys) #status
                         predicted\
   152,155,173,176       #binding_site zinc (Cys, Cys, His, Cys) #status
                         predicted\
   259                   #binding_site phosphate (Ser) (covalent) (by
                         protein kinase C) #status experimental\
   268                   #binding_site phosphate (Thr) (covalent) (by
                         autophosphorylation) #status experimental\
   375                   #active_site Lys #status predicted\
   499                   #binding_site phosphate (Ser) (covalent) (by
                         protein kinase C) #status predicted
SUMMARY         #length 648 #molecular_weight 73051

SEQUENCE
              5        10        15        20        25        30
    1 M E H I Q G A W K T I S N G F G F K D A V F D G S S C I S P
   31 T I V Q Q F G Y Q R R A S D D G K L T D P S K T S N T I R V
   61 F L P N K Q R T V V N V R N G M S L H D C L M K A L K V R G
   91 L Q P E C C A V F R L L H E H K G K K A R L D W N T D A A S
  121 L I G E E L Q V D F L D H V P L T T H N F A R K T F L K L A
  151 F C D I C Q K F L L N G F R C Q T C G Y K F H E H C S T K V
  181 P T M C V D W S N I R Q L L L F P N S T I G D S G V P A L P
  211 S L T M R R M R E S V S R M P V S S Q H R Y S T P H A F T F
  241 N T S S P S S E G S L S Q R Q R S T S T P N V H M V S T T L
  271 P V D S R M I E D A I R S H S E S A S P S A L S S S P N N L
  301 S P T G W S Q P K T P V P A Q R E R A P V S G T Q E K N K I
  331 R P R G Q R D S S Y Y W E I E A S E V M L S T R I G S G S F
  361 G T V Y K G K W H G D V A V K I L K V V D P T P E Q F Q A F
  391 R N E V A V L R K T R H V N I L L F M G Y M T K D N L A I V
  421 T Q W C E G S S L Y K H L H V Q E T K F Q M F Q L I D I A R
  451 Q T A Q G M D Y L H A K N I I H R D M K S N N I F L H E G L
  481 T V K I G D F G L A T V K S R W S G S Q Q V E Q P T G S V L
  511 W M A P E V I R M Q D N N P F S F Q S D V Y S Y G I V L Y E
  541 L M T G E L P Y S H I N N R D Q I I F M V G R G Y A S P D L
  571 S K L Y K N C P K A M K R L V A D C V K K V K E E R P L F P
  601 Q I L S S I E L L Q H S L P K I N R S A S E P S L H R A A H
  631 T E D I N A C T L T T S P R L P V F
