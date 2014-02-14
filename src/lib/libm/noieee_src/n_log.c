/*	$OpenBSD: n_log.c,v 1.8 2009/10/27 23:59:29 deraadt Exp $	*/
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <math.h>
#include <errno.h>

#include "mathimpl.h"

/* Table-driven natural logarithm.
 *
 * This code was derived, with minor modifications, from:
 *	Peter Tang, "Table-Driven Implementation of the
 *	Logarithm in IEEE Floating-Point arithmetic." ACM Trans.
 *	Math Software, vol 16. no 4, pp 378-400, Dec 1990).
 *
 * Calculates log(2^m*F*(1+f/F)), |f/j| <= 1/256,
 * where F = j/128 for j an integer in [0, 128].
 *
 * log(2^m) = log2_hi*m + log2_tail*m
 * since m is an integer, the dominant term is exact.
 * m has at most 10 digits (for subnormal numbers),
 * and log2_hi has 11 trailing zero bits.
 *
 * log(F) = logF_hi[j] + logF_lo[j] is in tabular form in log_table.h
 * logF_hi[] + 512 is exact.
 *
 * log(1+f/F) = 2*f/(2*F + f) + 1/12 * (2*f/(2*F + f))**3 + ...
 * the leading term is calculated to extra precision in two
 * parts, the larger of which adds exactly to the dominant
 * m and F terms.
 * There are two cases:
 *	1. when m, j are non-zero (m | j), use absolute
 *	   precision for the leading term.
 *	2. when m = j = 0, |1-x| < 1/256, and log(x) ~= (x-1).
 *	   In this case, use a relative precision of 24 bits.
 * (This is done differently in the original paper)
 *
 * Special cases:
 *	0	return signalling -Inf
 *	neg	return signalling NaN
 *	+Inf	return +Inf
*/

#if defined(__vax__)
#define _IEEE		0
#define TRUNC(x)	x = (double) (float) (x)
#else
#define _IEEE		1
#define endian		(((*(int *) &one)) ? 1 : 0)
#define TRUNC(x)	*(((int *) &x) + endian) &= 0xf8000000
#define infnan(x)	0.0
#endif

#define N 128

/* Table of log(Fj) = logF_head[j] + logF_tail[j], for Fj = 1+j/128.
 * Used for generation of extend precision logarithms.
 * The constant 35184372088832 is 2^45, so the divide is exact.
 * It ensures correct reading of logF_head, even for inaccurate
 * decimal-to-binary conversion routines.  (Everybody gets the
 * right answer for integers less than 2^53.)
 * Values for log(F) were generated using error < 10^-57 absolute
 * with the bc -l package.
*/
static const double	A1 = 	  .08333333333333178827;
static const double	A2 = 	  .01250000000377174923;
static const double	A3 =	 .002232139987919447809;
static const double	A4 =	.0004348877777076145742;

static const double logF_head[N+1] = {
	0.,
	.007782140442060381246,
	.015504186535963526694,
	.023167059281547608406,
	.030771658666765233647,
	.038318864302141264488,
	.045809536031242714670,
	.053244514518837604555,
	.060624621816486978786,
	.067950661908525944454,
	.075223421237524235039,
	.082443669210988446138,
	.089612158689760690322,
	.096729626458454731618,
	.103796793681567578460,
	.110814366340264314203,
	.117783035656430001836,
	.124703478501032805070,
	.131576357788617315236,
	.138402322859292326029,
	.145182009844575077295,
	.151916042025732167530,
	.158605030176659056451,
	.165249572895390883786,
	.171850256926518341060,
	.178407657472689606947,
	.184922338493834104156,
	.191394852999565046047,
	.197825743329758552135,
	.204215541428766300668,
	.210564769107350002741,
	.216873938300523150246,
	.223143551314024080056,
	.229374101064877322642,
	.235566071312860003672,
	.241719936886966024758,
	.247836163904594286577,
	.253915209980732470285,
	.259957524436686071567,
	.265963548496984003577,
	.271933715484010463114,
	.277868451003087102435,
	.283768173130738432519,
	.289633292582948342896,
	.295464212893421063199,
	.301261330578199704177,
	.307025035294827830512,
	.312755710004239517729,
	.318453731118097493890,
	.324119468654316733591,
	.329753286372579168528,
	.335355541920762334484,
	.340926586970454081892,
	.346466767346100823488,
	.351976423156884266063,
	.357455888922231679316,
	.362905493689140712376,
	.368325561158599157352,
	.373716409793814818840,
	.379078352934811846353,
	.384411698910298582632,
	.389716751140440464951,
	.394993808240542421117,
	.400243164127459749579,
	.405465108107819105498,
	.410659924985338875558,
	.415827895143593195825,
	.420969294644237379543,
	.426084395310681429691,
	.431173464818130014464,
	.436236766774527495726,
	.441274560805140936281,
	.446287102628048160113,
	.451274644139630254358,
	.456237433481874177232,
	.461175715122408291790,
	.466089729924533457960,
	.470979715219073113985,
	.475845904869856894947,
	.480688529345570714212,
	.485507815781602403149,
	.490303988045525329653,
	.495077266798034543171,
	.499827869556611403822,
	.504556010751912253908,
	.509261901790523552335,
	.513945751101346104405,
	.518607764208354637958,
	.523248143765158602036,
	.527867089620485785417,
	.532464798869114019908,
	.537041465897345915436,
	.541597282432121573947,
	.546132437597407260909,
	.550647117952394182793,
	.555141507540611200965,
	.559615787935399566777,
	.564070138285387656651,
	.568504735352689749561,
	.572919753562018740922,
	.577315365035246941260,
	.581691739635061821900,
	.586049045003164792433,
	.590387446602107957005,
	.594707107746216934174,
	.599008189645246602594,
	.603290851438941899687,
	.607555250224322662688,
	.611801541106615331955,
	.616029877215623855590,
	.620240409751204424537,
	.624433288012369303032,
	.628608659422752680256,
	.632766669570628437213,
	.636907462236194987781,
	.641031179420679109171,
	.645137961373620782978,
	.649227946625615004450,
	.653301272011958644725,
	.657358072709030238911,
	.661398482245203922502,
	.665422632544505177065,
	.669430653942981734871,
	.673422675212350441142,
	.677398823590920073911,
	.681359224807238206267,
	.685304003098281100392,
	.689233281238557538017,
	.693147180560117703862
};

static const double logF_tail[N+1] = {
	0.,
	-.00000000000000543229938420049,
	 .00000000000000172745674997061,
	-.00000000000001323017818229233,
	-.00000000000001154527628289872,
	-.00000000000000466529469958300,
	 .00000000000005148849572685810,
	-.00000000000002532168943117445,
	-.00000000000005213620639136504,
	-.00000000000001819506003016881,
	 .00000000000006329065958724544,
	 .00000000000008614512936087814,
	-.00000000000007355770219435028,
	 .00000000000009638067658552277,
	 .00000000000007598636597194141,
	 .00000000000002579999128306990,
	-.00000000000004654729747598444,
	-.00000000000007556920687451336,
	 .00000000000010195735223708472,
	-.00000000000017319034406422306,
	-.00000000000007718001336828098,
	 .00000000000010980754099855238,
	-.00000000000002047235780046195,
	-.00000000000008372091099235912,
	 .00000000000014088127937111135,
	 .00000000000012869017157588257,
	 .00000000000017788850778198106,
	 .00000000000006440856150696891,
	 .00000000000016132822667240822,
	-.00000000000007540916511956188,
	-.00000000000000036507188831790,
	 .00000000000009120937249914984,
	 .00000000000018567570959796010,
	-.00000000000003149265065191483,
	-.00000000000009309459495196889,
	 .00000000000017914338601329117,
	-.00000000000001302979717330866,
	 .00000000000023097385217586939,
	 .00000000000023999540484211737,
	 .00000000000015393776174455408,
	-.00000000000036870428315837678,
	 .00000000000036920375082080089,
	-.00000000000009383417223663699,
	 .00000000000009433398189512690,
	 .00000000000041481318704258568,
	-.00000000000003792316480209314,
	 .00000000000008403156304792424,
	-.00000000000034262934348285429,
	 .00000000000043712191957429145,
	-.00000000000010475750058776541,
	-.00000000000011118671389559323,
	 .00000000000037549577257259853,
	 .00000000000013912841212197565,
	 .00000000000010775743037572640,
	 .00000000000029391859187648000,
	-.00000000000042790509060060774,
	 .00000000000022774076114039555,
	 .00000000000010849569622967912,
	-.00000000000023073801945705758,
	 .00000000000015761203773969435,
	 .00000000000003345710269544082,
	-.00000000000041525158063436123,
	 .00000000000032655698896907146,
	-.00000000000044704265010452446,
	 .00000000000034527647952039772,
	-.00000000000007048962392109746,
	 .00000000000011776978751369214,
	-.00000000000010774341461609578,
	 .00000000000021863343293215910,
	 .00000000000024132639491333131,
	 .00000000000039057462209830700,
	-.00000000000026570679203560751,
	 .00000000000037135141919592021,
	-.00000000000017166921336082431,
	-.00000000000028658285157914353,
	-.00000000000023812542263446809,
	 .00000000000006576659768580062,
	-.00000000000028210143846181267,
	 .00000000000010701931762114254,
	 .00000000000018119346366441110,
	 .00000000000009840465278232627,
	-.00000000000033149150282752542,
	-.00000000000018302857356041668,
	-.00000000000016207400156744949,
	 .00000000000048303314949553201,
	-.00000000000071560553172382115,
	 .00000000000088821239518571855,
	-.00000000000030900580513238244,
	-.00000000000061076551972851496,
	 .00000000000035659969663347830,
	 .00000000000035782396591276383,
	-.00000000000046226087001544578,
	 .00000000000062279762917225156,
	 .00000000000072838947272065741,
	 .00000000000026809646615211673,
	-.00000000000010960825046059278,
	 .00000000000002311949383800537,
	-.00000000000058469058005299247,
	-.00000000000002103748251144494,
	-.00000000000023323182945587408,
	-.00000000000042333694288141916,
	-.00000000000043933937969737844,
	 .00000000000041341647073835565,
	 .00000000000006841763641591466,
	 .00000000000047585534004430641,
	 .00000000000083679678674757695,
	-.00000000000085763734646658640,
	 .00000000000021913281229340092,
	-.00000000000062242842536431148,
	-.00000000000010983594325438430,
	 .00000000000065310431377633651,
	-.00000000000047580199021710769,
	-.00000000000037854251265457040,
	 .00000000000040939233218678664,
	 .00000000000087424383914858291,
	 .00000000000025218188456842882,
	-.00000000000003608131360422557,
	-.00000000000050518555924280902,
	 .00000000000078699403323355317,
	-.00000000000067020876961949060,
	 .00000000000016108575753932458,
	 .00000000000058527188436251509,
	-.00000000000035246757297904791,
	-.00000000000018372084495629058,
	 .00000000000088606689813494916,
	 .00000000000066486268071468700,
	 .00000000000063831615170646519,
	 .00000000000025144230728376072,
	-.00000000000017239444525614834
};

double
log(double x)
{
	int m, j;
	double F, f, g, q, u, u2, v, zero = 0.0, one = 1.0;
	volatile double u1;

	/* Catch special cases */
	if (x <= 0)
		if (_IEEE && x == zero)	/* log(0) = -Inf */
			return (-one/zero);
		else if (_IEEE)		/* log(neg) = NaN */
			return (zero/zero);
		else if (x == zero)	/* NOT REACHED IF _IEEE */
			return (infnan(-ERANGE));
		else
			return (infnan(EDOM));
	else if (!finite(x))
		if (_IEEE)		/* x = NaN, Inf */
			return (x+x);
		else
			return (infnan(ERANGE));

	/* Argument reduction: 1 <= g < 2; x/2^m = g;	*/
	/* y = F*(1 + f/F) for |f| <= 2^-8		*/

	m = logb(x);
	g = ldexp(x, -m);
	if (_IEEE && m == -1022) {
		j = logb(g);
		m += j;
		g = ldexp(g, -j);
	}
	j = N*(g-1) + .5;
	F = (1.0/N) * j + 1;	/* F*128 is an integer in [128, 512] */
	f = g - F;

	/* Approximate expansion for log(1+f/F) ~= u + q */
	g = 1/(2*F+f);
	u = 2*f*g;
	v = u*u;
	q = u*v*(A1 + v*(A2 + v*(A3 + v*A4)));

    /* case 1: u1 = u rounded to 2^-43 absolute.  Since u < 2^-8,
     * 	       u1 has at most 35 bits, and F*u1 is exact, as F has < 8 bits.
     *         It also adds exactly to |m*log2_hi + log_F_head[j] | < 750
    */
	if (m | j) {
		u1 = u + 513;
		u1 -= 513;
	}

    /* case 2:	|1-x| < 1/256. The m- and j- dependent terms are zero;
     * 		u1 = u to 24 bits.
    */
	else {
		u1 = u;
		TRUNC(u1);
	}
	u2 = (2.0*(f - F*u1) - u1*f) * g;
			/* u1 + u2 = 2f/(2F+f) to extra precision.	*/

	/* log(x) = log(2^m*F*(1+f/F)) =				*/
	/* (m*log2_hi+logF_head[j]+u1) + (m*log2_lo+logF_tail[j]+q);	*/
	/* (exact) + (tiny)						*/

	u1 += m*logF_head[N] + logF_head[j];		/* exact */
	u2 = (u2 + logF_tail[j]) + q;			/* tiny */
	u2 += logF_tail[N]*m;
	return (u1 + u2);
}

/*
 * Extra precision variant, returning struct {double a, b;};
 * log(x) = a+b to 63 bits, with a rounded to 26 bits.
 */
struct Double
__log__D(double x)
{
	int m, j;
	double F, f, g, q, u, v, u2, one = 1.0;
	volatile double u1;
	struct Double r;

	/* Argument reduction: 1 <= g < 2; x/2^m = g;	*/
	/* y = F*(1 + f/F) for |f| <= 2^-8		*/

	m = logb(x);
	g = ldexp(x, -m);
	if (_IEEE && m == -1022) {
		j = logb(g);
		m += j;
		g = ldexp(g, -j);
	}
	j = N*(g-1) + .5;
	F = (1.0/N) * j + 1;
	f = g - F;

	g = 1/(2*F+f);
	u = 2*f*g;
	v = u*u;
	q = u*v*(A1 + v*(A2 + v*(A3 + v*A4)));
	if (m | j) {
		u1 = u + 513;
		u1 -= 513;
	}
	else {
		u1 = u;
		TRUNC(u1);
	}
	u2 = (2.0*(f - F*u1) - u1*f) * g;

	u1 += m*logF_head[N] + logF_head[j];

	u2 +=  logF_tail[j]; u2 += q;
	u2 += logF_tail[N]*m;
	r.a = u1 + u2;			/* Only difference is here */
	TRUNC(r.a);
	r.b = (u1 - r.a) + u2;
	return (r);
}
