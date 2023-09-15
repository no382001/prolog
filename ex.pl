% Excercises

/*
Prolog programozási NZH M1 és M2 mintafeladatok
===============================================

5. M1 mintafeladat segédeljárása:
Írjon Prolog nyelven egy olyan eljárást, amely előállítja egy konstans
értékét egy helyettesítési lista alapján. A helyettesítési lista minden
eleme Név−Szám alakú, ahol Szám a Név névkonstans helyettesítési értéke. Egy
számkonstans helyettesítési értéke önmaga, egy a helyettesítési listában nem
szereplő atom helyettesítési érteke pedig 0. Ha egy névkonstans többször
szerepel a helyettesítési listában, akkor az első előfordulást szabad csak
figyelembe venni.
% helyettesitese(+K, +HL, ?E): A K konstansnak a HL behelyettesítési
% lista szerinti értéke E.
Példák:
| ?− helyettesitese(y, [x−1,y−2,z−3], H). −−−−> H = 2 ? ; no
| ?− helyettesitese(u, [x−1,y−2,z−3], H). −−−−> H = 0 ? ; no
| ?− helyettesitese(x, [x−1,z−3,x−2], H). −−−−> H = 1 ? ; no
| ?− helyettesitese(4, [x−1,y−2,z−3], H). −−−−> H = 4 ? ; no
*/

helyettesitese(_,[],_) :- fail.
helyettesitese(X,[_|Es],H) :- helyettesitese(X,Es,H).
helyettesitese(X,[X-H|_],H).



onlys([]).
onlys([s|Es]) :- onlys(Es).
