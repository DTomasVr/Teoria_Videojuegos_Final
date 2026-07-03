#pragma once

// Modo de mezcla al dibujar. Se traduce a SDL en los .cpp (los headers no incluyen
// SDL). Alpha = transparencia normal; Add = aditivo (brilla, ideal para chispas y
// gore "llamativo", DOCX); Mod = multiplicativo; None = opaco.
enum class BlendMode { None, Alpha, Add, Mod };
