MY FRIEND PEDRO (2014 Flash) -> PSP    |  ALPHA 1.1  (man 1)
==========================================================

DIEU KHIEN (moi ngon tay giu mot nhom nut, choi duoc cung luc)
  Analog trai/phai : chay
  Analog len       : nhay (giu de nhay cao)
  Analog xuong     : cui/lan (CHUA lam)
  Nut mat          : NGAM 8 huong: Triangle = len, Cross = xuong,
                     Square = trai, Circle = phai (bam 2 nut de ngam cheo).
                     Khong bam nut nao: ngam theo huong dang quay mat.
  R                : ban
  L                : slow-motion (giu; ton nang luong, giet dich de hoi)
  D-pad phai/trai  : doi vu khi tien/lui  [alpha: co san ca 7 vu khi]
  D-pad xuong      : nap dan
  Select           : pause / choi lai khi chet hoac thang man
  Start            : thoat

MUC TIEU
  Bat dau tren mai nha. Ban vo cua so mai (skylight) de roi xuong quan an,
  giet het 11 ten gangster roi di den cuoi hem ben phai.

BUILD (Windows + WSL, da cai pspdev)
  psp-pacman -S sdl2 sdl2_image sdl2_mixer     # chi can lan dau
  cd pedro-psp
  make dist
  -> copy NOI DUNG thu muc dist/ (EBOOT.PBP + thu muc assets) vao PSP/GAME/Pedro/
  -> hoac mo dist/EBOOT.PBP bang PPSSPP de choi thu

NEU make BAO LOI
  Copy nguyen van loi gui lai. Cho de hong nhat la dong LIBS trong Makefile
  (ten thu vien SDL2 tren pspdev co the khac mot chut).

DA LAM (alpha 1)
  - Nen man 1 va mat na va cham dung theo hinh vector goc (ghep tu SWF)
  - Nhan vat, 7 vu khi, gangster, am thanh va nhac that tu game
  - Vat ly dung thong so goc (chay 6, nhay 5, trong luc 0.4, slow-mo 0.3)
  - Animation that cua gangster tu SWF: dung tho, di bo, ngam ban, nga ra sau/nga up khi chet
  - Lua dau nong sung (muzzle flash) cho nguoi choi va dich
  - Dich AI don gian: tien lai, ban co do lech; headshot x3; mau dich 20

CHUA CO
  - Cac man 2-11 va boss, menu, vat pham nhat duoc (dan/medipack)
  - Lan, leo tuong, thang may, animation chet cua nguoi choi
  - Cua so mai goc co hieu ung vo, mau, shell dan
  - Test tren may that: ban nay moi chay thu logic tren may tinh

LUU Y: tai nguyen game thuoc DeadToast Entertainment. Chi dung ca nhan,
dung phat tan ban port.

ANH XMB
  ICON0.PNG (144x80) = thumb game tren XMB (logo My Friend Pedro)
  PIC1.PNG (480x272) = anh nen khi chon game (sung + chuoi)
  Duoc gan san trong Makefile, EBOOT.PBP se tu dong nhung 2 anh nay.
