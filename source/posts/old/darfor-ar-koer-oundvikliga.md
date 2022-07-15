---
categories:
- name: computing
  value: Computing
- name: vetenskap-science
  value: Vetenskap (science)
date: '2015-01-31'
link: https://kraulis.wordpress.com/2015/01/31/darfor-ar-koer-oundvikliga/
name: darfor-ar-koer-oundvikliga
path: /2015/01/31/darfor-ar-koer-oundvikliga/
tags:
- name: koer
  value: "k\xF6er"
- name: ngi
  value: NGI
- name: scilifelab
  value: SciLifeLab
- name: simulering
  value: simulering
title: "D\xE4rf\xF6r \xE4r k\xF6er oundvikliga"
type: post
---
*This blog post is also available [in English](/2015/01/31/why-queues-are-inevitable/).*

Vi klagar gärna på köer. Varför ska vi behöva vänta? Visar inte t.ex. vårdköerna att för snåla resurser läggs på sjukvården? Jag har tittat litet närmare på detta problem.

**Min slutsats: Nej, vi är nog inte beredda att betala vad det kostar att avskaffa köerna.** Mitt resultat bygger på några enkla förutsättningar, och är tillämpligt på många olika typer av verksamheter. Jag har använt datorsimuleringar för att räkna på problemet. Siffrorna talar sitt tydliga språk: **Det köfria samhället är en orimlig utopi.**

Jag jobbar i en serviceverksamhet ([National Genomics Infrastructure in Sweden](https://portal.scilifelab.se/genomics/) som befinner sig inom [SciLifeLab](http://www.scilifelab.se/)), där vi processar prover inskickade av forskare i hela Sverige och levererar data (DNA-sekvenserna) tillbaka till dem. **Det vanligaste klagomålet på oss är att vi har för långa köer. Det glunkas om att [Karolinska Institutet](http://ki.se/), där många av våra användare finns, funderar på att öppna en egen service, för att, som en KI-representant sa till mig, "forskarna ska kunna komma ner och få sina prover körda direkt utan någon kö."** Jag började fundera på det. Är den målsättningen rimlig?

Det finns en mycket väl utvecklad teori om detta, främst inom det fält som heter [Discrete Event Simulation](http://en.wikipedia.org/wiki/Discrete_event_simulation), men de grundläggande dragen är så enkla att jag programmerade det följande själv. Källkod och andra filer finns på mitt GitHub-repo [queue_simulation](https://github.com/pekrau/queue_simulation) för den som vill titta på detaljerna.

**Ett helt grundläggande antagande som jag gör är att uppdragen kommer till serviceverksamheten helt oberoende av varandra. Det finns ingen tidsbokning, planering eller flockbeteende.** En patient blir sjuk helt oberoende av andra, eller en forskare levererar ett prov till NGI helt oberoende av andra forskare. Då kan man använda en så kallad [slumptalsgenerator](http://sv.wikipedia.org/wiki/Slumptalsgenerator), en slags datorbaserad tärning, för att i ett datorprogram simulera processen.

Så vad händer om vi simulerar en serviceverksamhet dit det kommer ett antal uppdrag litet när som helst? Här är en visuell representation:

[![Simulation of task execution and queue. 50% utilization of resources.](/files/queue_50.png)](/files/queue_50.png) Simulation of task execution and queue. 50% utilization of resources.

I denna figur löper tiden horisontellt. Varje uppdrag visas i sin egen rad. Följande färger används:

- **Svart** visar den tid det tar att utföra det uppdraget. Jag har valt att sätta den tiden till ett och samma värde för alla uppdrag, för enkelhetens skull.
- **Röd** visar den tid som uppdraget måste vänta innan det kan börja utföras. Detta sker när det finns andra uppdrag som hunnit före till verksamheten, och som därför behandlas före.
- **Vit** för markeringarna i den nedersta raden, som visar vid vilken tid varje uppdrag inkommit till verksamheten. **Notera den ojämnt fördelade utspridningen!** Det är så det blir om uppdragen inkommer rent slumpmässigt. Det är denna ojämnhet som är grundorsaken till de resultat jag kommer att beskriva. Denna effekt, som beror på slumpen, är ingen slump, ironiskt nog. Det är så verkligheten fungerar: slumpmässigt placerade punkter ger en ojämn fördelning.

I denna simuleringen har jag valt ett så stort inflöde av uppdrag så att **ungefär 50% av den totala kapaciteten** hos verksamheten utnyttjas. Verksamheten står alltså utan några uppdrag att utföra hälften av tiden. **Redan här kan man se något mycket intressant: Det finns ett antal tillfällen när de inkommande uppdragen tvingas vänta i kö.** Närmare bestämt 35 av de 70 uppdragen i denna figur får vänta på sin tur, medan resten kan börja processas direkt när de inkommer.

**Det betyder alltså att även om man har *dubbelt* så stor kapacitet som man strikt sett egentligen behöver, så måste ungefär hälften av uppdragen ändå vänta i kö!**

Om vi är ekonomer och administratörer, eller den som faktiskt betalar kalaset, så kanske vi vill ha en mer effektiv verksamhet, så att **den arbetar, låt oss säga, 90% av sin kapacitet**, och står sysslolös endast 10% av tiden. Det kan ju verka rimligt? Följande händer då:

[![Simulation of task execution and queue. 90% utilization of resources.](/files/queue_90.png)](/files/queue_90.png) Simulation of task execution and queue. 90% utilization of resources.

Usch då! En himla massa rött, vilket betyder lång väntan för många uppdrag.

- Den stora majoriteten av uppdrag råkar ut för kö. I det här fallet 87 av 100.
- De som råkar ut för kö ligger i den länge: i genomsnitt 19.1 dagar (jämfört med de 5 dagar som ett uppdrag tar att utföra; den tiden satte jag godtyckligt i denna simuleringen).
- Notera de långa diagonalerna av svarta block: Det visar att verksamheten arbetar utan uppehåll under långa perioder.

Om detta skulle handla om en akutmottagning, så skulle nog massmedia publicera katastrofreportage om de förfärliga köerna, och personalen skulle riskera utbrändhet. Och då finns det trots allt 10% glapp i systemet!

**Slutsats: En verksamhet som inte kan styra eller planera inflödet av uppdrag kommer att få orimliga köer om antalet inkomna uppdrag närmar sig den totala kapaciteten. Köerna är inte ett tecken på att kapaciteten rent principiellt är för låg! Det finns ju 10% som inte används, totalt sett. Ändå kommer alla att tycka att det är katastrof. Detta är en viktig men ofta förbisedd insikt.**

Om vi istället tittar på det som ovannämnde KI-representanten eftersträvar: En verksamhet som är i det närmaste utan köer. Hur uppnår vi det? Låt oss lägga **utnyttjandegraden på ett extremt lågt värde: 20%**. Så här ser det ut:

[![Simulation of task execution and queue. 20% utilization of resources.](/files/queue_20.png)](/files/queue_20.png) Simulation of task execution and queue. 20% utilization of resources.

Här blir det nästan köfritt; mycket litet rött syns. Men ändå litet grann! Och vi ser också långa sträckor där mycket litet händer: Verksamheten rullar tummarna. Den som betalar för verksamheten blir nog litet fundersam. **Lyxen att bli av med köerna är nämligen just det: en lyx. Verksamheten är 5 gånger större än den principiellt behöver vara, det är det som 20% utnyttjandegrad innebär.**

Man skulle självklart kunna gå mer på djupet i dessa frågor, och visa grafer som relaterar förekomst av kö, eller medellängd på kön, till hur stor nyttjandegrad man har. Men jag avstår i detta inlägg, det är tillräckligt långt som det är. Man kan också diskutera om man kan förändra det grundläggande antagandet jag gör i analysen, nämligen att uppdragen inkommer vid slumpmässiga tidpunkter. Visst, man kan i vissa fall (dock inte akut sjukhusvård) tänka sig andra system, som t.ex. tidsbokning, som skulle minska eländet. Men de lösningarna har egna problem som det skulle leda för långt att här resonera om.

**Jag vill avsluta med följande uppmaning till de som klagar på långa köer, i vård eller i andra sammanhang: Är ni beredda att betala för den överkapacitet som behövs för att uppnå frånvaron av köer?** För även om verksamheten skulle betalas av andra, rent tekniskt, så ska man inte inbilla sig att det inte kostar samhällsekonomin något. Har man en verksamhet vars kapacitet är långt större än vad den principiellt behöver vara, så innebär det dödtid, och därmed slöseri: Resurser felallokeras. Används till ingen nytta. Kunde ha använts till annat. Betänk det innan ni börjar gnälla.

