# Задача 33: ABI радара Android ARM64

Исследовано 2026-11-08 по локальному `build/icon-research/libGame.so`.
SHA256: `4C6A7445E30B27AFDDA781302E4DB9BAC89C28FC1181B68B1EEF16F84D6A282E`.
Адреса ниже — ELF virtual addresses, к ним прибавляется load bias. Предпочитать
`dlsym` экспортов. Исследование статическое; вызовы нового прохода на Quest пока
не проверялись. Сборка игры должна совпасть с исследованным бинарником.

## Что действительно рисует стоковый HUD

`CHud::DrawRadar()` (`0x5115f4`) вызывает:

1. `CRadar::DrawMap()` — вызов `0x511698`.
2. Спрайты обрамления/эффектов виджета радара — между картой и маркерами.
3. `CRadar::DrawBlips(-1.0f)` — вызов `0x511b9c`.

`DrawMap()` обновляет cached sin/cos (`0x5184b8`), вычисляет дальность по
скорости/транспорту, записывает `m_radarRange` (`0x518598`) и
`vec2DRadarOrigin` (`0x5185d8`). При допустимых условиях делает tail call в
`DrawRadarMap()` (`0x51863c`). Внутри есть ранние выходы, в том числе по
области игрока; вызов не гарантирует отрисовку карты в интерьере.

`DrawRadarMap()`:

- вызывает `DrawRadarMask()` (`0x518934`);
- вызывает `StreamRadarSections(int,int)` (`0x5189d8`);
- задаёт RenderWare render states;
- рисует девять соседних тайлов через `DrawRadarSection(int,int,int)`
  (`0x518aa4`–`0x518b24`), затем gang overlay и отдельные авиационные элементы.

Карта не является готовой круглой текстурой. Круг появляется в процессе
маскирования. Тайл строится в координатах радара, обрезается и переводится в
экранные координаты; маркеры рисуются отдельным проходом.

## Экспорты и ABI

| ELF VA | Символ для dlsym | C++ тип функции |
|---|---|---|
| 0x5115f4 | `_ZN4CHud9DrawRadarEv` | `void (*)()` |
| 0x5184ac | `_ZN6CRadar7DrawMapEv` | `void (*)()` |
| 0x518904 | `_ZN6CRadar12DrawRadarMapEv` | `void (*)()` |
| 0x51e96c | `_ZN6CRadar13DrawRadarMaskEv` | `void (*)()` |
| 0x518e44 | `_ZN6CRadar9DrawBlipsEf` | `void (*)(float)` |
| 0x51dfc4 | `_ZN6CRadar16DrawRadarSectionEiii` | `void (*)(int,int,int)` |
| 0x51e360 | `_ZN6CRadar19DrawRadarSectionMapEii5CRectf` | `void (*)(int,int,CRect,float)` |
| 0x51dcd8 | `_ZN6CRadar13ClipRadarPolyEP9CVector2DPKS0_` | `int (*)(Vec2*,const Vec2*)` |
| 0x519ac4 | `_ZN6CRadar15LimitRadarPointER9CVector2D` | `float (*)(Vec2&)` |
| 0x5199c4 | `_ZN6CRadar32TransformRadarPointToScreenSpaceER9CVector2DRKS0_` | `void (*)(Vec2&,const Vec2&)` |
| 0x519a64 | `_ZN6CRadar35TransformRealWorldPointToRadarSpaceER9CVector2DRKS0_` | `void (*)(Vec2&,const Vec2&)` |
| 0x51d810 | `_ZN6CRadar17RequestMapSectionEii` | `void (*)(int,int)` |
| 0x51d950 | `_ZN6CRadar16RemoveMapSectionEii` | `void (*)(int,int)` |
| 0x51d998 | `_ZN6CRadar19StreamRadarSectionsEii` | `void (*)(int,int)` |

Эти методы CRadar статические: скрытого `this` нет. `Vec2` — два float,
ссылки передаются указателями x0/x1. `CRect` в `DrawRadarSectionMap` передаётся
по значению как HFA из четырёх float: s0–s3; последний float alpha — s4,
индексы тайла — w0/w1. Нельзя объявлять этот аргумент указателем.
Порядок и смысл полей CRect следует дополнительно подтвердить перед вызовом;
имя CRect само по себе этого не доказывает.

Экспортируемое состояние:

| ELF VA | Символ | Наблюдаемый формат |
|---|---|---|
| 0xa69620 | `_ZN6CRadar16vec2DRadarOriginE` | два float |
| 0xa69628 | `_ZN6CRadar12m_radarRangeE` | float |
| 0xa6962c | `_ZN6CRadar11m_radarRectE` | прямоугольник, layout требует проверки |
| 0xa660a8 | `gRadarTextures` | 144 указателя, индекс `y*12+x` |
| 0xa66740 | `_ZN6CRadar13ms_RadarTraceE` | массив маркеров; layout пока не восстановлен |
| 0xa69648 | `_ZN6CRadar16RadarBlipSpritesE` | спрайты маркеров |

## Где круг и где прямоугольник

`ClipRadarPoly()` уже работает с квадратом: в `0x51dd24`–`0x51dd3c`
проверяется `max(abs(x),abs(y)) <= 1`. Затем идёт обрезка по границам.
Следовательно, заменять эту функцию ради прямоугольного экрана необязательно:
нормализованный квадрат можно отобразить в прямоугольник.

`DrawRadarMask()` рисует геометрию маски через `RwIm2DRenderIndexedPrimitive`.
Включает depth write и меняет alpha-test states. Простое пропускание вызова
без воспроизведения нужных render states ещё не проверено и не считается
готовым решением.

`LimitRadarPoint()` отдельно ограничивает маркеры единичной окружностью:
возвращает исходную длину в s0 и при длине >1 нормализует точку на месте
(`0x519ae8`–`0x519afc`). В режиме frontend (`gMobileMenu+0x8c`, bit 0)
нормализация пропускается. Для КПК нужен свой scoped clamp к границе
прямоугольника с сохранением семантики возвращаемой длины.

`TransformRadarPointToScreenSpace()` в gameplay использует виджет радара:
его прямоугольник читается по смещениям +0x2c/+0x30/+0x34/+0x38.
Получается `screenX = centreX + x*halfWidth`,
`screenY = centreY - y*halfHeight`. Frontend-ветка использует
`gMobileMenu+0x78/+0x7c/+0x80`. Изменение frontend-флага меняет одновременно
несколько ветвей поведения и для отдельного КПК не рекомендуется.

## Тайлы и отдельный вызов DrawRadarSectionMap

Сетка 12x12, размер тайла 500 мировых единиц (видно в DrawRadarSection).
`DrawRadarSectionMap()` рисует уже загруженный тайл в переданный CRect:
проверяет индексы 0..11, берёт `gRadarTextures[y*12+x]`, при null выходит,
задаёт texture raster, создаёт четыре вершины CSprite2d и отправляет quad.
Сам не делает streaming, не рисует игрока, цели или остальные blips.
Один такой вызов не заменяет полноценный радар.

`RequestMapSection()` загружает отсутствующую текстуру и увеличивает её
refcount; повторный запрос уже загруженного тайла выходит без инкремента.
`RemoveMapSection()` уничтожает ссылку и обнуляет слот. Stock streaming
обходит 12x12 и сохраняет окрестность 3x3, выгружая остальные тайлы.
Нельзя независимо вызывать его с двумя разными центрами для HUD и КПК:
проходы будут выгружать тайлы друг друга.

## Рекомендуемый следующий шаг реализации

Сохранить stock путь и добавить отдельный проход КПК в собственный
RenderWare raster с тем же кольцом поколений, что и глаза/HUD. На игровом
потоке переиспользовать расчёт origin/range/heading и stock blips, а внутри
scoped-контекста КПК заменить маску и экранное преобразование, а также
ограничение маркеров. Обычный HUD вне этого контекста должен использовать
исходные функции. Геометрия карты должна сохранять одинаковый масштаб
по мировым X/Y; для 16:10 нужно расширение видимой области, а не растяжение.
При выходе за покрытие 3x3 потребуется согласованная политика streaming.

До кода проверить caller-ы DrawCoordBlip/DrawEntityBlip и все пути, где
маркер обходит LimitRadarPoint; проверить game render-state restoration и
безопасные прологи для trampolines. На Quest проверить реальные тайлы,
маркер игрока, цели миссий, края экрана, интерьеры, автомобиль и отсутствие
влияния на pause map. Эти проверки не заменяются успешной Android-сборкой.

Корпус КПК — отдельная задача геометрии: параллелепипед, лицевая сторона
16:10, короткая сторона чуть уже ладони; экран немного утоплен в корпус.
Текущая плоская рамка и HUD crop пользовательским MVP не приняты.

## Итерация отдельного overlay

`PdaRadar.inl` заменяет вызов DrawBlips с подменой gMobileMenu: читает
250 записей ms_RadarTrace (stride 0x30), координаты +8, sprite +0x28,
flags +0x29, entity ref +4. Автомобили/педы/объекты разрешаются через
штатные CPools::Get*, координатные blips учитывают CEntryExit.
Игровые спрайты из RadarBlipSprites рисуются CSprite2d::Draw с четырьмя
углами: в ARM64 его аргументы идут BL, BR, TL, TR. Игрок — sprite 2,
север — sprite 4. Метки проецируются с масштабом тайлов (4/3 px/m),
обрезаются в повёрнутом прямоугольном viewport и возвращаются в координаты
исходной текстуры. Для обычных значков компенсируется поворот текстуры;
стрелка игрока следует его heading, карта — heading стереокамеры/HMD.

Сборка и установка прошли. Точный паритет со stock ещё не подтверждён:
специализированные типы pickup/spotlight/airstrip, short-range/mission/fade
фильтрация и формы без спрайта по высоте требуют последующей интеграции.
Новая отрисовка обычных mission blips и entity blips требует проверки в Quest.
