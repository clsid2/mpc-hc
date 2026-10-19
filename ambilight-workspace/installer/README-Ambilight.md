# MPC-HC Ambilight

Buka MPC-HC Ambilight dari shortcut Desktop atau Start Menu, lalu buka video.
Klik Ambilight di toolbar bawah, tepat di kiri kontrol volume/audio.
Slider dan angka langsung mengubah tampilan, termasuk saat pause, dan
tersimpan otomatis. Tidak perlu OK atau Apply.

Default sesuai preset pilihan:
- Enable ambient light: aktif
- Intensity: 53
- Blur: 55
- Edge size: 27
- Spread: 55
- Spread fade start / curve: 15 / 35
- Brightness / Contrast / Colors / Saturation: 100
- Debanding: 0
- Top / Right / Bottom / Left: aktif

Reset mengembalikan angka ke preset tersebut. Pengaturan yang sudah disimpan
tetap dipakai setelah update. Installer memasang aplikasi untuk pengguna
Windows saat ini, menyediakan shortcut dan uninstaller.
Halaman Select Destination Location selalu muncul, termasuk saat install
ulang. Klik Browse untuk memilih folder tujuan sebelum melanjutkan.
Update build Ambilight dilakukan lewat installer build berikutnya.

Build komunitas ini berbasis MPC-HC 2.8.4 dan MPC Video Renderer, dengan panel
Ambilight dan Gaussian blur bertingkat. Video utama tetap tajam.
Efek mengisi ruang kosong akibat perbedaan rasio video dan layar.
Video pas layar tidak punya ruang efek. Bar hitam di dalam file belum dideteksi.
HDR, Windows HDR output, RTX Video HDR, D3D9 dan stereo interlaced bypass efek.

Lisensi player: COPYING.txt; renderer: MPCVR-LICENSE.txt.
Source kedua komponen dan script build ada di folder Source.
Fork GitHub:
https://github.com/teguhsatria892/mpc-hc
https://github.com/teguhsatria892/VideoRenderer
Source Ambilight tersedia di branch ambilight pada kedua fork tersebut.

Pengaturan player disimpan di mpc-hc64.ini di folder aplikasi. Pengaturan
renderer memakai HKCU\Software\MPC-BE Filters\MPC Video Renderer Ambilight.
Uninstall mempertahankan preferensi agar bisa dipakai saat install kembali.
