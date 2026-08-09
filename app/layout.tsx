import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "EZW Image Compression",
  description: "Wavelet-based image compression using the EZW algorithm",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
