# Анимации рук

Руки используют только `skeletal/hands.glb`: один меш, 25 костей, до восьми
весов и 14 именованных клипов. Старые модели/покадровые блоки UXRH и fallback
удалены. Ошибка загрузки GLB явно отключает руки и записывается в журнал.

Исходник для редактирования: `source/Hands.blend`, Blender 4.5 LTS. В нём
сохранены риг и все Actions; вспомогательного оружия и других сценовых мешей нет.
GLB, исходник и `BigHandsAlbedo.png` хранятся через Git LFS. После клонирования
нужно выполнить `git lfs install` и `git lfs pull`.

| Состояние | Клип и управление |
| --- | --- |
| Свободная рука | Grip / Point / Fist, непрерывная поверхность grip/trigger |
| Зажим предмета, радио, зажигание | Pinch |
| Руль | Grip по аналоговому grip |
| Лазание | Grip, закрытая поза |
| Мяч | OpenPalm |
| Обычный пистолетный хват | GripPistolShoot по trigger |
| Поддержка пистолета | GripPistolOffhand по grip |
| Deagle | GripDeagle / GripDeagleOffhand |
| Винтовка | GripRifle |
| Дробовик | GripShotgun / GripShotgunOffhand |
| Гранатомёт | Прежние GripPistol / GripPistolShoot |
| Beckon | Сохранён, нового игрового назначения нет |

Выбор и приоритеты находятся в `native/src/HandAnimation.h`, проигрывание и
смешивание костей — в `HandSkeleton.cpp`. Прогресс клипов задаётся аналоговыми
входами, вращения смешиваются SLERP. Кольт и перезарядка этой задачей не меняются.

Воспроизводимый экспорт, профиль GLB и результаты проверок описаны в
[docs/hand-skeleton.md](../../docs/hand-skeleton.md).
