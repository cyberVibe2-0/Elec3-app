/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#include "bezier.h"
#include "graphicsutils.h"
#include "../debugdialog.h"
#include <qmath.h>
#include <limits>

/////////////////////////////////////////////

// borrowed liberally from the following sources:
//
// http://processingjs.nihongoresources.com/bezierinfo/
// http://www.tinaja.com/text/bezmath.html,
// http://www.lemoda.net/maths/bezier-length/index.html,
// http://www.flong.com/texts/code/shapers_bez/
// http://steve.hollasch.net/cgindex/curves/cbezarclen.html

/////////////////////////////////////////////

// utilities from http://www.flong.com/texts/code/shapers_bez/

constexpr double B0 (double t) noexcept {
	return (1-t)*(1-t)*(1-t);
}
constexpr double B1 (double t) noexcept {
	return  3*t* (1-t)*(1-t);
}
constexpr double B2 (double t) noexcept {
	return 3*t*t* (1-t);
}
constexpr double B3 (double t) noexcept {
	return t*t*t;
}

/////////////////////////////////////////////

// utilities from http://processingjs.nihongoresources.com/bezierinfo/sketchsource.php?sketch=cubicGaussQuadrature

constexpr double base3(double t, double p1, double p2, double p3, double p4) noexcept
{
	double t1 = -3*p1 + 9*p2 - 9*p3 + 3*p4;
	double t2 = t*t1 + 6*p1 - 12*p2 + 6*p3;
	return t*t2 - 3*p1 + 3*p2;
}

// Legendre-Gauss abscissae (xi values, defined at i=n as the roots of the nth order Legendre polynomial Pn(x))
constexpr double Tvalues[25][24] = {
	{},
	{},
	{-0.5773502691896257310588680411456152796745,0.5773502691896257310588680411456152796745},
	{0.0000000000000000000000000000000000000000,-0.7745966692414834042779148148838430643082,0.7745966692414834042779148148838430643082},
	{-0.3399810435848562573113440521410666406155,0.3399810435848562573113440521410666406155,-0.8611363115940525725378051902225706726313,0.8611363115940525725378051902225706726313},
	{0.0000000000000000000000000000000000000000,-0.5384693101056831077144693153968546539545,0.5384693101056831077144693153968546539545,-0.9061798459386639637003213465504813939333,0.9061798459386639637003213465504813939333},
	{0.6612093864662644815410885712481103837490,-0.6612093864662644815410885712481103837490,-0.2386191860831969047129774708082550205290,0.2386191860831969047129774708082550205290,-0.9324695142031520500580654697841964662075,0.9324695142031520500580654697841964662075},
	{0.0000000000000000000000000000000000000000,0.4058451513773971841558818596240598708391,-0.4058451513773971841558818596240598708391,-0.7415311855993944600839995473506860435009,0.7415311855993944600839995473506860435009,-0.9491079123427584862682238053821492940187,0.9491079123427584862682238053821492940187},
	{-0.1834346424956498078362443493460887111723,0.1834346424956498078362443493460887111723,-0.5255324099163289908176466269651427865028,0.5255324099163289908176466269651427865028,-0.7966664774136267279658341067261062562466,0.7966664774136267279658341067261062562466,-0.9602898564975362871720676594122778624296,0.9602898564975362871720676594122778624296},
	{0.0000000000000000000000000000000000000000,-0.8360311073266357695388251158874481916428,0.8360311073266357695388251158874481916428,-0.9681602395076260858530758923734538257122,0.9681602395076260858530758923734538257122,-0.3242534234038089158147499801998492330313,0.3242534234038089158147499801998492330313,-0.6133714327005903577116896485676988959312,0.6133714327005903577116896485676988959312},
	{-0.1488743389816312157059030596428783610463,0.1488743389816312157059030596428783610463,-0.4333953941292472133994806426926515996456,0.4333953941292472133994806426926515996456,-0.6794095682990244355892173189204186201096,0.6794095682990244355892173189204186201096,-0.8650633666889845363456856830453034490347,0.8650633666889845363456856830453034490347,-0.9739065285171717434309357486199587583542,0.9739065285171717434309357486199587583542},
	{0.0000000000000000000000000000000000000000,-0.2695431559523449593918087430211016908288,0.2695431559523449593918087430211016908288,-0.5190961292068118071441062966187018901110,0.5190961292068118071441062966187018901110,-0.7301520055740493564400139803183265030384,0.7301520055740493564400139803183265030384,-0.8870625997680953167545681026240345090628,0.8870625997680953167545681026240345090628,-0.9782286581460569729884468870295677334070,0.9782286581460569729884468870295677334070},
	{-0.1252334085114689132822718420356977730989,0.1252334085114689132822718420356977730989,-0.3678314989981801841345543380157323554158,0.3678314989981801841345543380157323554158,-0.5873179542866174829285341729701030999422,0.5873179542866174829285341729701030999422,-0.7699026741943046925342741815256886184216,0.7699026741943046925342741815256886184216,-0.9041172563704749087776235683122649788857,0.9041172563704749087776235683122649788857,-0.9815606342467192435563561048184055835009,0.9815606342467192435563561048184055835009},
	{0.0000000000000000000000000000000000000000,-0.2304583159551348015003924274424207396805,0.2304583159551348015003924274424207396805,-0.4484927510364468683512484403763664886355,0.4484927510364468683512484403763664886355,-0.6423493394403402279024817289609927684069,0.6423493394403402279024817289609927684069,-0.8015780907333098781464286730624735355377,0.8015780907333098781464286730624735355377,-0.9175983992229779229177211163914762437344,0.9175983992229779229177211163914762437344,-0.9841830547185881350458203087328001856804,0.9841830547185881350458203087328001856804},
	{-0.1080549487073436676354276642086915671825,0.1080549487073436676354276642086915671825,-0.3191123689278897446186533670697826892138,0.3191123689278897446186533670697826892138,-0.5152486363581540995681962158414535224438,0.5152486363581540995681962158414535224438,-0.6872929048116854788830210054584313184023,0.6872929048116854788830210054584313184023,-0.8272013150697650196718768711434677243233,0.8272013150697650196718768711434677243233,-0.9284348836635735180422557277779560536146,0.9284348836635735180422557277779560536146,-0.9862838086968123141318187663273420184851,0.9862838086968123141318187663273420184851},
	{0.0000000000000000000000000000000000000000,-0.2011940939974345143870237961891689337790,0.2011940939974345143870237961891689337790,-0.3941513470775633853904196257644798606634,0.3941513470775633853904196257644798606634,-0.5709721726085388304738899023504927754402,0.5709721726085388304738899023504927754402,-0.7244177313601700696210627938853576779366,0.7244177313601700696210627938853576779366,-0.8482065834104272061821916395274456590414,0.8482065834104272061821916395274456590414,-0.9372733924007059513883177714888006448746,0.9372733924007059513883177714888006448746,-0.9879925180204853774057482951320707798004,0.9879925180204853774057482951320707798004},
	{-0.0950125098376374405129141109682677779347,0.0950125098376374405129141109682677779347,-0.2816035507792589154263396267197094857693,0.2816035507792589154263396267197094857693,-0.4580167776572273696800152720243204385042,0.4580167776572273696800152720243204385042,-0.6178762444026437705701937375124543905258,0.6178762444026437705701937375124543905258,-0.7554044083550029986540153004170861095190,0.7554044083550029986540153004170861095190,-0.8656312023878317551961458775622304528952,0.8656312023878317551961458775622304528952,-0.9445750230732326002680565579794347286224,0.9445750230732326002680565579794347286224,-0.9894009349916499385102497399202547967434,0.9894009349916499385102497399202547967434},
	{0.0000000000000000000000000000000000000000,-0.1784841814958478545261044700964703224599,0.1784841814958478545261044700964703224599,-0.3512317634538763000406902392569463700056,0.3512317634538763000406902392569463700056,-0.5126905370864769384553483178024180233479,0.5126905370864769384553483178024180233479,-0.6576711592166907260903485621383879333735,0.6576711592166907260903485621383879333735,-0.7815140038968013680431567991035990417004,0.7815140038968013680431567991035990417004,-0.8802391537269859123071569229068700224161,0.8802391537269859123071569229068700224161,-0.9506755217687677950166857954172883182764,0.9506755217687677950166857954172883182764,-0.9905754753144173641032921295845881104469,0.9905754753144173641032921295845881104469},
	{-0.0847750130417353059408824833553808275610,0.0847750130417353059408824833553808275610,-0.2518862256915054831374334298743633553386,0.2518862256915054831374334298743633553386,-0.4117511614628426297457508553634397685528,0.4117511614628426297457508553634397685528,-0.5597708310739475390249708652845583856106,0.5597708310739475390249708652845583856106,-0.6916870430603532238222896921797655522823,0.6916870430603532238222896921797655522823,-0.8037049589725231424353069087374024093151,0.8037049589725231424353069087374024093151,-0.8926024664975557021406871172075625509024,0.8926024664975557021406871172075625509024,-0.9558239495713977129653926567698363214731,0.9558239495713977129653926567698363214731,-0.9915651684209308980300079383596312254667,0.9915651684209308980300079383596312254667},
	{0.0000000000000000000000000000000000000000,-0.1603586456402253668240831530056311748922,0.1603586456402253668240831530056311748922,-0.3165640999636298302810644145210972055793,0.3165640999636298302810644145210972055793,-0.4645707413759609383241411251219687983394,0.4645707413759609383241411251219687983394,-0.6005453046616809897884081692609470337629,0.6005453046616809897884081692609470337629,-0.7209661773352293856476080691209062933922,0.7209661773352293856476080691209062933922,-0.8227146565371428188484514976153150200844,0.8227146565371428188484514976153150200844,-0.9031559036148179009373393455462064594030,0.9031559036148179009373393455462064594030,-0.9602081521348300174878431789693422615528,0.9602081521348300174878431789693422615528,-0.9924068438435843519940249279898125678301,0.9924068438435843519940249279898125678301},
	{-0.0765265211334973383117130651953630149364,0.0765265211334973383117130651953630149364,-0.2277858511416450681963397073559463024139,0.2277858511416450681963397073559463024139,-0.3737060887154195487624974703066982328892,0.3737060887154195487624974703066982328892,-0.5108670019508271264996324134699534624815,0.5108670019508271264996324134699534624815,-0.6360536807265150249790508496516849845648,0.6360536807265150249790508496516849845648,-0.7463319064601507957235071444301865994930,0.7463319064601507957235071444301865994930,-0.8391169718222187823286617458506952971220,0.8391169718222187823286617458506952971220,-0.9122344282513259461353527512983419001102,0.9122344282513259461353527512983419001102,-0.9639719272779138092843709273438435047865,0.9639719272779138092843709273438435047865,-0.9931285991850948846604296704754233360291,0.9931285991850948846604296704754233360291},
	{0.0000000000000000000000000000000000000000,-0.1455618541608950933241573011400760151446,0.1455618541608950933241573011400760151446,-0.2880213168024011172185794293909566476941,0.2880213168024011172185794293909566476941,-0.4243421202074387776903563462838064879179,0.4243421202074387776903563462838064879179,-0.5516188358872198271853903861483559012413,0.5516188358872198271853903861483559012413,-0.6671388041974123384036943207320291548967,0.6671388041974123384036943207320291548967,-0.7684399634756778896260698275000322610140,0.7684399634756778896260698275000322610140,-0.8533633645833172964856316866644192487001,0.8533633645833172964856316866644192487001,-0.9200993341504007938524978271743748337030,0.9200993341504007938524978271743748337030,-0.9672268385663063128276917268522083759308,0.9672268385663063128276917268522083759308,-0.9937521706203894522602126926358323544264,0.9937521706203894522602126926358323544264},
	{-0.0697392733197222253194169638845778536052,0.0697392733197222253194169638845778536052,-0.2078604266882212725509049278116435743868,0.2078604266882212725509049278116435743868,-0.3419358208920842412403828802780481055379,0.3419358208920842412403828802780481055379,-0.4693558379867570073962212973128771409392,0.4693558379867570073962212973128771409392,-0.5876404035069116016387624767958186566830,0.5876404035069116016387624767958186566830,-0.6944872631866827461522007070016115903854,0.6944872631866827461522007070016115903854,-0.7878168059792081123759999172762036323547,0.7878168059792081123759999172762036323547,-0.8658125777203001804949167308222968131304,0.8658125777203001804949167308222968131304,-0.9269567721871739829353487039043102413416,0.9269567721871739829353487039043102413416,-0.9700604978354286922481719557254109531641,0.9700604978354286922481719557254109531641,-0.9942945854823992402060639506089501082897,0.9942945854823992402060639506089501082897},
	{0.0000000000000000000000000000000000000000,-0.1332568242984661088801345840693102218211,0.1332568242984661088801345840693102218211,-0.2641356809703449548543119362875586375594,0.2641356809703449548543119362875586375594,-0.3903010380302908144400930723350029438734,0.3903010380302908144400930723350029438734,-0.5095014778460075222099590064317453652620,0.5095014778460075222099590064317453652620,-0.6196098757636461229481028567533940076828,0.6196098757636461229481028567533940076828,-0.7186613631319501704908248029823880642653,0.7186613631319501704908248029823880642653,-0.8048884016188398993207897547108586877584,0.8048884016188398993207897547108586877584,-0.8767523582704416229560706597112584859133,0.8767523582704416229560706597112584859133,-0.9329710868260161493736859483760781586170,0.9329710868260161493736859483760781586170,-0.9725424712181152120393790028174407780170,0.9725424712181152120393790028174407780170,-0.9947693349975521570627279288601130247116,0.9947693349975521570627279288601130247116},
	{-0.0640568928626056299791002857091370970011,0.0640568928626056299791002857091370970011,-0.1911188674736163106704367464772076345980,0.1911188674736163106704367464772076345980,-0.3150426796961633968408023065421730279922,0.3150426796961633968408023065421730279922,-0.4337935076260451272567308933503227308393,0.4337935076260451272567308933503227308393,-0.5454214713888395626995020393223967403173,0.5454214713888395626995020393223967403173,-0.6480936519369755455244330732966773211956,0.6480936519369755455244330732966773211956,-0.7401241915785543579175964623573236167431,0.7401241915785543579175964623573236167431,-0.8200019859739029470802051946520805358887,0.8200019859739029470802051946520805358887,-0.8864155270044010714869386902137193828821,0.8864155270044010714869386902137193828821,-0.9382745520027327978951348086411599069834,0.9382745520027327978951348086411599069834,-0.9747285559713094738043537290650419890881,0.9747285559713094738043537290650419890881,-0.9951872199970213106468008845695294439793,0.9951872199970213106468008845695294439793}
};

// Legendre-Gauss weights (wi values, defined by a function linked to in the Bezier primer article)
constexpr double Cvalues[25][24] = {
	{},
	{},
	{1.0000000000000000000000000000000000000000,1.0000000000000000000000000000000000000000},
	{0.8888888888888888395456433499930426478386,0.5555555555555555802271783250034786760807,0.5555555555555555802271783250034786760807},
	{0.6521451548625460947761212082696147263050,0.6521451548625460947761212082696147263050,0.3478548451374538497127275604725582525134,0.3478548451374538497127275604725582525134},
	{0.5688888888888888883954564334999304264784,0.4786286704993664709029133064177585765719,0.4786286704993664709029133064177585765719,0.2369268850561890848993584768322762101889,0.2369268850561890848993584768322762101889},
	{0.3607615730481386062677984227775596082211,0.3607615730481386062677984227775596082211,0.4679139345726910370615314604947343468666,0.4679139345726910370615314604947343468666,0.1713244923791703566706701167277060449123,0.1713244923791703566706701167277060449123},
	{0.4179591836734694032529091600736137479544,0.3818300505051189230876218516641529276967,0.3818300505051189230876218516641529276967,0.2797053914892766446342875497066415846348,0.2797053914892766446342875497066415846348,0.1294849661688697028960604029634851031005,0.1294849661688697028960604029634851031005},
	{0.3626837833783619902128236844873754307628,0.3626837833783619902128236844873754307628,0.3137066458778872690693617641954915598035,0.3137066458778872690693617641954915598035,0.2223810344533744820516574236535234376788,0.2223810344533744820516574236535234376788,0.1012285362903762586661571276636095717549,0.1012285362903762586661571276636095717549},
	{0.3302393550012597822629345500899944454432,0.1806481606948573959137149813614087179303,0.1806481606948573959137149813614087179303,0.0812743883615744122650426106702070683241,0.0812743883615744122650426106702070683241,0.3123470770400028628799304897256661206484,0.3123470770400028628799304897256661206484,0.2606106964029354378098446431977208703756,0.2606106964029354378098446431977208703756},
	{0.2955242247147528700246255084493895992637,0.2955242247147528700246255084493895992637,0.2692667193099963496294435572053771466017,0.2692667193099963496294435572053771466017,0.2190863625159820415877476307286997325718,0.2190863625159820415877476307286997325718,0.1494513491505805868886369580650352872908,0.1494513491505805868886369580650352872908,0.0666713443086881379917585377370414789766,0.0666713443086881379917585377370414789766},
	{0.2729250867779006162194832540990319103003,0.2628045445102466515230332788632949814200,0.2628045445102466515230332788632949814200,0.2331937645919904822378043718344997614622,0.2331937645919904822378043718344997614622,0.1862902109277342621584949711177614517510,0.1862902109277342621584949711177614517510,0.1255803694649046120535018644659430719912,0.1255803694649046120535018644659430719912,0.0556685671161736631007421749472996452823,0.0556685671161736631007421749472996452823},
	{0.2491470458134027732288728884668671526015,0.2491470458134027732288728884668671526015,0.2334925365383548057085505433860816992819,0.2334925365383548057085505433860816992819,0.2031674267230659247651658461109036579728,0.2031674267230659247651658461109036579728,0.1600783285433462210800570346691529266536,0.1600783285433462210800570346691529266536,0.1069393259953184266430881166343169752508,0.1069393259953184266430881166343169752508,0.0471753363865118277575838590109924552962,0.0471753363865118277575838590109924552962},
	{0.2325515532308738975153517003491288051009,0.2262831802628972321933531475224299356341,0.2262831802628972321933531475224299356341,0.2078160475368885096170146198346628807485,0.2078160475368885096170146198346628807485,0.1781459807619457380578609217991470359266,0.1781459807619457380578609217991470359266,0.1388735102197872495199959530509659089148,0.1388735102197872495199959530509659089148,0.0921214998377284516317686779984796885401,0.0921214998377284516317686779984796885401,0.0404840047653158771612247335269785253331,0.0404840047653158771612247335269785253331},
	{0.2152638534631577948985636794532183557749,0.2152638534631577948985636794532183557749,0.2051984637212956041896205761076998896897,0.2051984637212956041896205761076998896897,0.1855383974779378219999159682629397138953,0.1855383974779378219999159682629397138953,0.1572031671581935463599677404999965801835,0.1572031671581935463599677404999965801835,0.1215185706879031851679329179205524269491,0.1215185706879031851679329179205524269491,0.0801580871597602079292599341897584963590,0.0801580871597602079292599341897584963590,0.0351194603317518602714208952875196700916,0.0351194603317518602714208952875196700916},
	{0.2025782419255612865072180284187197685242,0.1984314853271115786093048427574103698134,0.1984314853271115786093048427574103698134,0.1861610000155622113293674146916600875556,0.1861610000155622113293674146916600875556,0.1662692058169939202105780395868350751698,0.1662692058169939202105780395868350751698,0.1395706779261543240000520427201990969479,0.1395706779261543240000520427201990969479,0.1071592204671719394948325998484506271780,0.1071592204671719394948325998484506271780,0.0703660474881081243747615872052847407758,0.0703660474881081243747615872052847407758,0.0307532419961172691358353148416426847689,0.0307532419961172691358353148416426847689},
	{0.1894506104550685021692402187909465283155,0.1894506104550685021692402187909465283155,0.1826034150449235837765371570640127174556,0.1826034150449235837765371570640127174556,0.1691565193950025358660127494658809155226,0.1691565193950025358660127494658809155226,0.1495959888165767359691216142891789786518,0.1495959888165767359691216142891789786518,0.1246289712555338768940060845125117339194,0.1246289712555338768940060845125117339194,0.0951585116824927856882254673109855502844,0.0951585116824927856882254673109855502844,0.0622535239386478936318702892549481475726,0.0622535239386478936318702892549481475726,0.0271524594117540964133272751723779947497,0.0271524594117540964133272751723779947497},
	{0.1794464703562065333031227964966092258692,0.1765627053669926449508409405098063871264,0.1765627053669926449508409405098063871264,0.1680041021564500358653759803928551264107,0.1680041021564500358653759803928551264107,0.1540457610768102836296122859494062140584,0.1540457610768102836296122859494062140584,0.1351363684685254751283167706787935458124,0.1351363684685254751283167706787935458124,0.1118838471934039680011352402289048768580,0.1118838471934039680011352402289048768580,0.0850361483171791776580761279547004960477,0.0850361483171791776580761279547004960477,0.0554595293739872027827253475606994470581,0.0554595293739872027827253475606994470581,0.0241483028685479314545681006620725383982,0.0241483028685479314545681006620725383982},
	{0.1691423829631436004383715498988749459386,0.1691423829631436004383715498988749459386,0.1642764837458327298325144738555536605418,0.1642764837458327298325144738555536605418,0.1546846751262652419622867228099494241178,0.1546846751262652419622867228099494241178,0.1406429146706506538855308008351130411029,0.1406429146706506538855308008351130411029,0.1225552067114784593471199514169711619616,0.1225552067114784593471199514169711619616,0.1009420441062871681703327908508072141558,0.1009420441062871681703327908508072141558,0.0764257302548890515847546112127020023763,0.0764257302548890515847546112127020023763,0.0497145488949697969549568199454370187595,0.0497145488949697969549568199454370187595,0.0216160135264833117019200869890482863411,0.0216160135264833117019200869890482863411},
	{0.1610544498487836984068621859478298574686,0.1589688433939543399375793342187535017729,0.1589688433939543399375793342187535017729,0.1527660420658596696075193221986410208046,0.1527660420658596696075193221986410208046,0.1426067021736066031678547005867585539818,0.1426067021736066031678547005867585539818,0.1287539625393362141547726196222356520593,0.1287539625393362141547726196222356520593,0.1115666455473339896409257221421285066754,0.1115666455473339896409257221421285066754,0.0914900216224499990280705219447554554790,0.0914900216224499990280705219447554554790,0.0690445427376412262931992813719261903316,0.0690445427376412262931992813719261903316,0.0448142267656995996194524423117400147021,0.0448142267656995996194524423117400147021,0.0194617882297264781221723950466184760444,0.0194617882297264781221723950466184760444},
	{0.1527533871307258372951309866039082407951,0.1527533871307258372951309866039082407951,0.1491729864726037413369397199858212843537,0.1491729864726037413369397199858212843537,0.1420961093183820411756101975697674788535,0.1420961093183820411756101975697674788535,0.1316886384491766370796739238357986323535,0.1316886384491766370796739238357986323535,0.1181945319615184120110029653005767613649,0.1181945319615184120110029653005767613649,0.1019301198172404415709380032240005675703,0.1019301198172404415709380032240005675703,0.0832767415767047547436874310733401216567,0.0832767415767047547436874310733401216567,0.0626720483341090678353069165495980996639,0.0626720483341090678353069165495980996639,0.0406014298003869386621822457072994438931,0.0406014298003869386621822457072994438931,0.0176140071391521178811867542890468030237,0.0176140071391521178811867542890468030237},
	{0.1460811336496904144777175815761438570917,0.1445244039899700461138110085812513716519,0.1445244039899700461138110085812513716519,0.1398873947910731496691028041823301464319,0.1398873947910731496691028041823301464319,0.1322689386333374683690777828815043903887,0.1322689386333374683690777828815043903887,0.1218314160537285334440227302366110961884,0.1218314160537285334440227302366110961884,0.1087972991671483785625085261017375160009,0.1087972991671483785625085261017375160009,0.0934444234560338621298214434318651910871,0.0934444234560338621298214434318651910871,0.0761001136283793039316591944043466355652,0.0761001136283793039316591944043466355652,0.0571344254268572049326735395879950374365,0.0571344254268572049326735395879950374365,0.0369537897708524937234741969405149575323,0.0369537897708524937234741969405149575323,0.0160172282577743345377552230957007850520,0.0160172282577743345377552230957007850520},
	{0.1392518728556319806966001806358690373600,0.1392518728556319806966001806358690373600,0.1365414983460151721050834794368711300194,0.1365414983460151721050834794368711300194,0.1311735047870623838139891859100316651165,0.1311735047870623838139891859100316651165,0.1232523768105124178928733158500108402222,0.1232523768105124178928733158500108402222,0.1129322960805392156435900119504367467016,0.1129322960805392156435900119504367467016,0.1004141444428809648581335522976587526500,0.1004141444428809648581335522976587526500,0.0859416062170677286236042391465161927044,0.0859416062170677286236042391465161927044,0.0697964684245204886048341563764552120119,0.0697964684245204886048341563764552120119,0.0522933351526832859712534684604179346934,0.0522933351526832859712534684604179346934,0.0337749015848141515006020085820637177676,0.0337749015848141515006020085820637177676,0.0146279952982721998810955454928262042813,0.0146279952982721998810955454928262042813},
	{0.1336545721861061852830943053049850277603,0.1324620394046966131984532921705977059901,0.1324620394046966131984532921705977059901,0.1289057221880821613169132433540653437376,0.1289057221880821613169132433540653437376,0.1230490843067295336776822978208656422794,0.1230490843067295336776822978208656422794,0.1149966402224113642960290349037677515298,0.1149966402224113642960290349037677515298,0.1048920914645414120824895576333801727742,0.1048920914645414120824895576333801727742,0.0929157660600351542612429511791560798883,0.0929157660600351542612429511791560798883,0.0792814117767189491248203125906002242118,0.0792814117767189491248203125906002242118,0.0642324214085258499151720457120973151177,0.0642324214085258499151720457120973151177,0.0480376717310846690356385124687221832573,0.0480376717310846690356385124687221832573,0.0309880058569794447631551292943186126649,0.0309880058569794447631551292943186126649,0.0134118594871417712993677540112003043760,0.0134118594871417712993677540112003043760},
	{0.1279381953467521593204025975865079089999,0.1279381953467521593204025975865079089999,0.1258374563468283025002847352880053222179,0.1258374563468283025002847352880053222179,0.1216704729278033914052770114722079597414,0.1216704729278033914052770114722079597414,0.1155056680537255991980671865348995197564,0.1155056680537255991980671865348995197564,0.1074442701159656343712356374453520402312,0.1074442701159656343712356374453520402312,0.0976186521041138843823858906034729443491,0.0976186521041138843823858906034729443491,0.0861901615319532743431096832864568568766,0.0861901615319532743431096832864568568766,0.0733464814110802998392557583429152145982,0.0733464814110802998392557583429152145982,0.0592985849154367833380163688161701429635,0.0592985849154367833380163688161701429635,0.0442774388174198077483545432642131345347,0.0442774388174198077483545432642131345347,0.0285313886289336633705904233693217975087,0.0285313886289336633705904233693217975087,0.0123412297999872001830201639904771582223,0.0123412297999872001830201639904771582223}
};

/////////////////////////////////////////////

Bezier::Bezier(QPointF cp0, QPointF cp1) : m_cp0(cp0), m_cp1(cp1), m_isEmpty(false)
{
}

Bezier::Bezier() : m_isEmpty(true) { }

void Bezier::clear()
{
	m_isEmpty = true;
}

void Bezier::set_cp0(QPointF cp0)
{
	m_cp0 = cp0;
	m_isEmpty = false;
}

void Bezier::set_cp1(QPointF cp1)
{
	m_cp1 = cp1;
	m_isEmpty = false;
}

void Bezier::set_endpoints(QPointF ep0, QPointF ep1)
{
	m_endpoint0 = ep0;
	m_endpoint1 = ep1;
}

Bezier Bezier::fromElement(QDomElement & element)
{
	Bezier bezier;
	if (element.tagName().compare("bezier") != 0) return bezier;

	QDomElement point = element.firstChildElement("cp0");
	if (point.isNull()) return bezier;

	double x = point.attribute("x").toDouble();
	double y = point.attribute("y").toDouble();
	bezier.set_cp0(QPointF(x, y));

	point = element.firstChildElement("cp1");
	x = point.attribute("x").toDouble();
	y = point.attribute("y").toDouble();
	bezier.set_cp1(QPointF(x, y));

	return bezier;
}

void Bezier::write(QXmlStreamWriter & streamWriter)
{
	if (isEmpty()) return;

	streamWriter.writeStartElement("bezier");

	streamWriter.writeStartElement("cp0");
	streamWriter.writeAttribute("x", QString::number(m_cp0.x()));
	streamWriter.writeAttribute("y", QString::number(m_cp0.y()));
	streamWriter.writeEndElement();
	streamWriter.writeStartElement("cp1");
	streamWriter.writeAttribute("x", QString::number(m_cp1.x()));
	streamWriter.writeAttribute("y", QString::number(m_cp1.y()));
	streamWriter.writeEndElement();

	streamWriter.writeEndElement();
}

bool Bezier::operator==(const Bezier & other) const
{
	if (m_isEmpty != other.isEmpty()) return false;
	if (m_cp0 != other.cp0()) return false;
	if (m_cp1 != other.cp1()) return false;

	return true;
}

bool Bezier::operator!=(const Bezier & other) const
{
	if (m_isEmpty != other.isEmpty()) return true;
	if (m_cp0 != other.cp0()) return true;
	if (m_cp1 != other.cp1()) return true;

	return false;
}

void Bezier::recalc(QPointF p)
{
	// http://www.flong.com/texts/code/shapers_bez/
	// http://www.lemoda.net/maths/bezier-length/index.html,

	// arbitrary but reasonable t-values for interior control points
	double t0 = 0.3;
	double t1 = 0.7;

	if (m_drag_cp0) {
		double x = (p.x() - m_endpoint0.x() * B0(t0) - m_cp1.x() * B2(t0) - m_endpoint1.x() * B3(t0)) / B1(t0);
		double y = (p.y() - m_endpoint0.y() * B0(t0) - m_cp1.y() * B2(t0) - m_endpoint1.y() * B3(t0)) / B1(t0);
		m_cp0 = QPointF(x, y);
	}
	else {
		double x = (p.x() - m_endpoint0.x() * B0(t1) - m_cp0.x() * B1(t1) - m_endpoint1.x() * B3(t1)) /  B2(t1);
		double y = (p.y() - m_endpoint0.y() * B0(t1) - m_cp0.y() * B1(t1) - m_endpoint1.y() * B3(t1)) /  B2(t1);
		m_cp1 = QPointF(x, y);
	}

	/*
	DebugDialog::debug(QString("ix:%1 p0x:%2,p0y:%3 p1x:%4,p1y:%5 px:%6,py:%7")
							.arg(m_drag_cp0)
							.arg(m_endpoint0.x())
							.arg(m_endpoint0.y())
							.arg(m_endpoint1.x())
							.arg(m_endpoint1.y())
							.arg(p.x())
							.arg(p.y())
							);
	*/

	m_isEmpty = false;
}

void Bezier::initToEnds(QPointF cp0, QPointF cp1)
{
    m_endpoint0 = cp0;
    m_cp0 = cp0;
    m_endpoint1 = cp1;
    m_cp1 = cp1;
	m_isEmpty = false;
}

double Bezier::xFromT(double t) const noexcept
{
	// http://www.lemoda.net/maths/bezier-length/index.html

	return m_endpoint0.x() * B0(t) + m_cp0.x() * B1(t) + m_cp1.x() * B2(t) + m_endpoint1.x() * B3(t);
}

double Bezier::xFromTPrime(double t) const
{
	return base3(t, m_endpoint0.x(), m_cp0.x(), m_cp1.x(), m_endpoint1.x());

}

double Bezier::yFromT(double t) const
{
	// http://www.lemoda.net/maths/bezier-length/index.html

	return m_endpoint0.y() * B0(t) + m_cp0.y() * B1(t) + m_cp1.y() * B2(t) + m_endpoint1.y() * B3(t);
}

void Bezier::split(double t, Bezier & left, Bezier & right) const
{
	// http://processingjs.nihongoresources.com/bezierinfo/sketchsource.php?sketch=CubicDeCasteljau

	// interpolate from 4 to 3 points
	QPointF p5((1-t)*m_endpoint0.x() + t*m_cp0.x(), (1-t)*m_endpoint0.y() + t*m_cp0.y());
	QPointF p6((1-t)*m_cp0.x() + t*m_cp1.x(), (1-t)*m_cp0.y() + t*m_cp1.y());
	QPointF p7((1-t)*m_cp1.x() + t*m_endpoint1.x(), (1-t)*m_cp1.y() + t*m_endpoint1.y());

	// interpolate from 3 to 2 points
	QPointF p8((1-t)*p5.x() + t*p6.x(), (1-t)*p5.y() + t*p6.y());
	QPointF p9((1-t)*p6.x() + t*p7.x(), (1-t)*p6.y() + t*p7.y());

	// interpolate from 2 points to 1 point
	QPointF p10((1-t)*p8.x() + t*p9.x(), (1-t)*p8.y() + t*p9.y());

	// we now have all the values we need to build the subcurves
	left.m_endpoint0 = m_endpoint0;
	left.m_cp0 = p5;
	left.m_cp1 = p8;
	right.m_endpoint0 = left.m_endpoint1 = p10;
	right.m_cp0 = p9;
	right.m_cp1 = p7;
	right.m_endpoint1 = m_endpoint1;
	left.m_isEmpty = right.m_isEmpty = false;
}

void Bezier::initControlIndex(QPointF p, double width)
{
	double t = findSplit(p, width);
	double totalLen = computeCubicCurveLength(1, 24);
	double len = computeCubicCurveLength(t, 24);
	//double d0 = GraphicsUtils::distanceSqd(p, m_cp0);
	//double d1 = GraphicsUtils::distanceSqd(p, m_cp1);
	m_drag_cp0 = (len <= totalLen / 2);
}

/**
 * Gauss quadrature for cubic Bezier curves
 */
double Bezier::computeCubicCurveLength(double z, int n) const
{
	// http://processingjs.nihongoresources.com/bezierinfo/sketchsource.php?sketch=cubicGaussQuadrature

	double z2 = z/2.0;
	double sum = 0;
	for (int i = 0; i < n; i++) {
		double corrected_t = z2 * Tvalues[n][i] + z2;
		sum += Cvalues[n][i] * cubicF(corrected_t);
	}
	return z2 * sum;
}

double Bezier::cubicF(double t) const noexcept
{
	double xbase = base3(t, m_endpoint0.x(), m_cp0.x(), m_cp1.x(), m_endpoint1.x());
	double ybase = base3(t, m_endpoint0.y(), m_cp0.y(), m_cp1.y(), m_endpoint1.y());
	double combined = xbase*xbase + ybase*ybase;
	return qSqrt(combined);
}

void Bezier::copy(const Bezier * other)
{
	if (other == NULL) {
		m_isEmpty = true;
		return;
	}

	m_cp0 = other->m_cp0;
	m_cp1 = other->m_cp1;
	m_endpoint0 = other->m_endpoint0;
	m_endpoint1 = other->m_endpoint1;
	m_isEmpty = other->m_isEmpty;
	m_drag_cp0 = other->m_drag_cp0;
}

double Bezier::findSplit(QPointF p, double minDistance) const
{
	double bestT = 0;
	double lastDistance = std::numeric_limits<int>::max();
	double blen = computeCubicCurveLength(1.0, 24);
	double increment = 1.0 / blen;
	double minDSqd = minDistance * minDistance;
	for (double t = 0; t <= 1; t += increment) {
		double x = xFromT(t);
		double y = yFromT(t);
		double d = GraphicsUtils::distanceSqd(p, QPointF(x, y));
		if (d >= lastDistance) {
			if (d > minDSqd) continue;

			break;
		}

		bestT = t;
		lastDistance = d;
	}

	return bestT;
}

void Bezier::translateToZero() {
	m_cp0 -= m_endpoint0;
	m_cp1 -= m_endpoint0;
	m_endpoint1 -= m_endpoint0;
	m_endpoint0 -= m_endpoint0;
}

void Bezier::translate(QPointF p) {
	m_cp0 += p;
	m_cp1 += p;
	m_endpoint1 += p;
	m_endpoint0 += p;
}

Bezier Bezier::join(const Bezier* other) const {
    if (!other || other->isEmpty()) {
        return {};
    } else {
        return join(*other);
    }
}
Bezier Bezier::join(const Bezier& other) const noexcept
{
	Bezier bezier;
	bool otherIsEmpty = other.isEmpty();

	if (isEmpty() && otherIsEmpty) {
		return bezier;
	}
	else {
		if (isEmpty()) {
			bezier.set_cp0(m_endpoint0);
			bezier.set_cp1(other.cp1());
		}
		else if (other.isEmpty()) {
			bezier.set_cp1(other.m_endpoint1);
			bezier.set_cp0(cp0());
		}
		else {
			bezier.set_cp0(cp0());
			bezier.set_cp1(other.cp1());
		}
	}

	return bezier;
}
