# Скелетные VR-руки GLB

Руки загружаются только из `vrhands/skeletal/hands.glb`: одна геометрия,
один skin, embedded PNG и 15 клипов. Старые модели и покадровые UXRH-анимации,
их загрузчик, CPU-деформация, динамические буферы и старый шейдер удалены.
UXRH радио к этой системе не относится и сохранён.

Успешная инициализация пишет `GLB GPU skinning ACTIVE` даже в player build.
При ошибке GLB/шейдера/текстуры руки отключаются с явным `GLB path FAILED`;
резервного пути больше нет. Builder отклоняет отсутствующий GLB и LFS pointer,
который ещё не заменён настоящим файлом.

## Исходники и Git LFS

- `assets/vrhands/source/Hands.blend`: исходник только с HandRight, ригом Hand
  и всеми 15 Actions; один меш, 25 deform-костей, один Armature modifier.
  Constraints/IK нет, до 7 весов на вершину. Обязательных правок рига не выявлено.
- `assets/vrhands/skeletal/hands.glb`: готовый runtime-ассет.
- `assets/vrhands/BigHandsAlbedo.png`: исходная текстура для экспорта.

Эти три бинарных файла хранятся через Git LFS; JSON manifest — обычный текст.
После клонирования: `git lfs install`, затем `git lfs pull`.
Исходная смешанная сцена `BothHands.blend` и её backups остаются локальными,
не перезаписываются и не публикуются. Подготовка hands-only копии выполняется
`tools/prepare_hand_source.py` из отдельной рабочей копии исходной сцены.
При этом удаляются вспомогательные меши оружия, камеры и неиспользуемые данные.

## Экспорт

Blender 4.5.15 LTS, рабочая копия обязательна:

```powershell
New-Item -ItemType Directory -Force .cache/hand-skeletal | Out-Null
Copy-Item assets/vrhands/source/Hands.blend .cache/hand-skeletal/Hands-export.blend
& 'C:/Program Files/Blender Foundation/Blender 4.5/blender.exe' --factory-startup -b .cache/hand-skeletal/Hands-export.blend --python-exit-code 1 --python tools/export_hand_glb.py
```

Экспортёр получает геометрию, UV-швы и triangulation непосредственно из Blender,
без UXRH-эталона. Через dependency graph вычисляются матрицы поз и сохраняются
локальные TRS относительно deform-родителя. Constraints/IK при их добавлении
запекаются в кости, не в геометрию. Shear, dual quaternion modifier и другие
модификаторы отвергаются явно. LINEAR-ключи костей записываются каждые 0,25 кадра;
постоянные каналы сокращаются до двух ключей. Все веса сохраняются через
`JOINTS_0/1` и `WEIGHTS_0/1`; геометрия между клипами не дублируется.

GLB использует метры и Y-up: `(x,y,z) Blender -> (.01*x,.01*z,-.01*y)`.
Scene placement меша исключён. В шейдере только для ассета рук применяется
обратное преобразование осей. Прежние базисы контроллеров, смещение кисти
5,5 см и точки взаимодействий сохранены. Rest pose и inverse bind matrices
проверяются на согласованность.

## Runtime

`HandSkeleton.cpp` использует cgltf 1.15 (MIT, лицензия рядом). Профиль:
один embedded GLB buffer, один skinned triangle mesh, до 64 узлов/32 joints,
до 8 весов, TRS-иерархия, inverse bind matrices, rotation/translation/scale
с LINEAR или STEP. Вращения интерполируются shortest-path SLERP.
CUBICSPLINE, sparse accessors, morphs, сжатие, расширения и неподдерживаемые
материалы явно отвергаются. Проверяются границы без переполнения, индексы,
веса, время, кватернионы, finite values, bind pose и циклы. Размер GLB ограничен
16 МиБ, бюджет парсера — 32 МиБ. Ошибка reload сохраняет загруженную библиотеку.

PNG 1024×1024 декодируется один раз через явный `dlopen(libjnigraphics.so)`;
временные PNG и пиксели затем освобождаются. Геометрия хранится в постоянных
VBO/EBO с отдельным VAO. На кадре обновляются небольшие позы и матрицы на стеке,
без файлового I/O и пересоздания мешей. Освещение и skin tone сохранены.

`HandAnimation::Select` и `FreeHand` сохраняют прежний выбор жестов, аналоговый
прогресс и приоритеты. Палитры обеих рук строятся из одного baked stereo
snapshot перед глазами; трекеры повторно между глазами не опрашиваются.
Отражение остаётся в прежнем базисе кисти; нормали используют inverse transpose
skin matrix, culling рук выключен. Ledge grip сохраняет прежнее распределение
фиксации ладони. Оружие, кольт и ручная перезарядка не меняются.

## Проверки

- `tools/test-hand-actions.py`: committed GLB, один меш, skin, все 15 клипов,
  embedded albedo, соответствие source hash, отсутствие старых моделей.
- `tools/test-hand-animation-runtime.py`: аналоговый выбор жестов и приоритеты.
- `tools/test-hand-skeleton.py`: production loader/sampler, все клипы, bind,
  hierarchy, 8 weights, SLERP, смешивание, STEP, translation/scale, крайние и
  нечисловые входы, непрерывность диагонали, 17 повреждённых GLB. Контрольная
  геометрия Blender создаётся только как QA fixture в `.cache`, не runtime.
- `tools/verify_hand_glb_blender.py`: повторный импорт в чистый Blender,
  материал, кости, Actions и начальные/средние/конечные позы.
- `tools/test-hand-skeleton-gpu.py`: production GLSL через WebGL2/ANGLE,
  transform feedback для обеих рук/глаз, normal/UV и ledge blending.
- `tools/test-hand-skeleton-quest.py`: установленный GLB, Android PNG decoder
  и production GLSL на Adreno 740 / OpenGL ES 3.2 без запуска игры.

14 ноября 2026 прежняя GLB-версия прошла все эти проверки: максимальная ошибка
геометрии относительно Blender — 0,168 мм; 50 688 host GPU сравнений и
409 416 сравнений на Quest. Пользователь подтвердил нормальное изображение
в игре, а захваченный запуск в 15:28:34 показал `GLB GPU skinning ACTIVE`,
25 joints, 2437 vertices, 904120 resident CPU bytes, без fallback.

До перехода runtime-right UXRH-анимации занимали 22 651 488 байт, обе стороны
локально — 45 302 976 байт, CPU storage библиотеки на host — 18 084 528 байт.
GLB с embedded PNG занимает около 1,49 МБ, CPU storage — менее 1 МБ;
GPU geometry buffers — 221 144 байт. Это размеры данных, не RSS Android и не
измерение ускорения FPS. Старые статические CPU-буферы рук (214 000 байт) удалены.

Дополнительный `test-vehicle-climb.py` ранее останавливался на старом ожидании
ровно 8 записей калибровок против расширенного неизменённого defaults-файла.
Его чтение геометрии переведено на GLB; сами игровые калибровки не менялись.
После удаления UXRH повторно прошли asset/selector/native sampler, Blender
roundtrip и host GPU проверки; также прошли `test-radio-assets.py` и
`test-ledge-climb.py`. Дополнительный неизменённый `test-pistol-support.py`
не компилирует свой harness: в нём отсутствует объявление `pairedweapon`,
используемое текущим production-кодом. Логика оружия этой задачей не менялась.

## Установка

Изменения hand assets устанавливаются `./tools/deploy-quest.ps1 -Full`.
Новый payload содержит GLB без старых моделей/клипов; штатная публикация дерева
удаляет их из активного каталога на Quest. Проверка ощущений и всех специальных
взаимодействий в шлеме остаётся пользовательской проверкой после обновления.


16 ноября 2026: добавлен `HandleRadio` для служебной рации (задача 97).
Указательный на боковой кнопке, большой вверх по противоположной грани.
Кадры 1…11 — удержание/нажатие; источник и GLB теперь содержат 15 клипов.
Предыдущие 14 конечных поз GLB совпадают точно; геометрия, веса и риг сохранены.
