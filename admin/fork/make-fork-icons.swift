// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Generates the Nextcloud Fork icons from the original Nextcloud artwork:
// - theme/colored/NextcloudFork-macOS-icon.svg (+ PNG sizes): the macOS app icon with a "fork" label
//   under the logo;
// - theme/{colored,black,white}/nextcloudfork/state-*.svg (+ PNG sizes): menu bar state icons, the
//   original glyph with a "fork" label below it. They stay square: Qt shows tray icons in a square
//   NSStatusItem and scales wider images down;
// - shell_integration/.../FileProviderExt/Assets.xcassets/FileProviderForkSymbol.symbolset: the symbol Finder
//   shows for the drive in "Locations", the Nextcloud logo symbol with the label below it.
// The label is drawn as glyph outlines of the rounded system font, so the result does not depend on the
// fonts available at build time. Run from the repository root: swift admin/fork/make-fork-icons.swift
// (needs rsvg-convert from librsvg for the PNG sizes).

import AppKit
import Foundation

let label = "fork"

/// SVG path data of `text` set in the rounded system font, with the baseline at `baselineY` and the
/// horizontal start at `originX` (SVG coordinates, y pointing down). Returns the path and its advance width.
func labelPath(_ text: String, fontSize: CGFloat, weight: NSFont.Weight, originX: CGFloat, baselineY: CGFloat)
    -> (path: String, width: CGFloat)
{
    var font = NSFont.systemFont(ofSize: fontSize, weight: weight)
    if let rounded = font.fontDescriptor.withDesign(.rounded) {
        font = NSFont(descriptor: rounded, size: fontSize) ?? font
    }
    let line = CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: [.font: font]))
    let width = CGFloat(CTLineGetTypographicBounds(line, nil, nil, nil))
    var d = ""
    let number = { (value: CGFloat) in String(format: "%.2f", value) }
    for run in CTLineGetGlyphRuns(line) as! [CTRun] {
        let runFont = (CTRunGetAttributes(run) as NSDictionary)[kCTFontAttributeName] as! CTFont
        let count = CTRunGetGlyphCount(run)
        var glyphs = [CGGlyph](repeating: 0, count: count)
        var positions = [CGPoint](repeating: .zero, count: count)
        CTRunGetGlyphs(run, CFRange(location: 0, length: count), &glyphs)
        CTRunGetPositions(run, CFRange(location: 0, length: count), &positions)
        for (glyph, position) in zip(glyphs, positions) {
            var transform = CGAffineTransform(a: 1, b: 0, c: 0, d: -1, tx: originX + position.x, ty: baselineY)
            guard let glyphPath = CTFontCreatePathForGlyph(runFont, glyph, &transform) else { continue }
            glyphPath.applyWithBlock { element in
                let points = element.pointee.points
                switch element.pointee.type {
                case .moveToPoint: d += "M\(number(points[0].x)) \(number(points[0].y))"
                case .addLineToPoint: d += "L\(number(points[0].x)) \(number(points[0].y))"
                case .addQuadCurveToPoint:
                    d += "Q\(number(points[0].x)) \(number(points[0].y)) \(number(points[1].x)) \(number(points[1].y))"
                case .addCurveToPoint:
                    d += "C\(number(points[0].x)) \(number(points[0].y)) \(number(points[1].x)) \(number(points[1].y)) "
                        + "\(number(points[2].x)) \(number(points[2].y))"
                case .closeSubpath: d += "Z"
                @unknown default: break
                }
            }
        }
    }
    return (d, width)
}

func read(_ path: String) -> String {
    guard let text = try? String(contentsOfFile: path, encoding: .utf8) else { fatalError("Cannot read \(path)") }
    return text
}

func write(_ text: String, to path: String) {
    try! FileManager.default.createDirectory(
        atPath: (path as NSString).deletingLastPathComponent, withIntermediateDirectories: true)
    try! text.write(toFile: path, atomically: true, encoding: .utf8)
}

/// Renders `svg` to `png` with the given height; the width follows the SVG's aspect ratio.
func renderPNG(_ svg: String, _ png: String, height: Int, width: Int? = nil) {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
    process.arguments = ["rsvg-convert", "-h", "\(height)"] + (width.map { ["-w", "\($0)"] } ?? []) + [svg, "-o", png]
    try! process.run()
    process.waitUntilExit()
    guard process.terminationStatus == 0 else { fatalError("rsvg-convert failed for \(svg)") }
}

/// The bounding box of the painted pixels of an SVG, in its viewBox units: the state glyphs do not fill
/// their 16x16 box (the Nextcloud logo uses its top half), so they are scaled by what they actually draw.
func paintedBounds(ofSVG svg: String, viewBoxWidth: CGFloat, viewBoxHeight: CGFloat) -> CGRect {
    let resolution = 20
    let png = NSTemporaryDirectory() + "fork-icon-bounds.png"
    renderPNG(svg, png, height: Int(viewBoxHeight) * resolution, width: Int(viewBoxWidth) * resolution)
    guard let bitmap = NSBitmapImageRep(data: try! Data(contentsOf: URL(fileURLWithPath: png))) else {
        fatalError("Cannot read \(png)")
    }
    var minX = bitmap.pixelsWide, minY = bitmap.pixelsHigh, maxX = -1, maxY = -1
    for y in 0..<bitmap.pixelsHigh {
        for x in 0..<bitmap.pixelsWide where (bitmap.colorAt(x: x, y: y)?.alphaComponent ?? 0) > 0.05 {
            minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
        }
    }
    guard maxX >= 0 else { return CGRect(x: 0, y: 0, width: viewBoxWidth, height: viewBoxHeight) }
    let unit = CGFloat(resolution)
    return CGRect(x: CGFloat(minX) / unit, y: CGFloat(minY) / unit,
                  width: CGFloat(maxX - minX + 1) / unit, height: CGFloat(maxY - minY + 1) / unit)
}

// MARK: - macOS app icon

let appIconSource = "theme/colored/Nextcloud-macOS-icon.svg"
let appIconTarget = "theme/colored/NextcloudFork-macOS-icon.svg"
// The logo moves up to make room for the label; the squircle spans y 100...924.
let logoCenterY = 410
let appLabel = labelPath(label, fontSize: 210, weight: .bold, originX: 0, baselineY: 790)
let appLabelPath = labelPath(label, fontSize: 210, weight: .bold, originX: 512 - appLabel.width / 2, baselineY: 790).path

var appIcon = read(appIconSource).replacingOccurrences(of: "cy=\"512\"", with: "cy=\"\(logoCenterY)\"")
let appLabelMarkup = """
        <g
            id="fork-label-shadow"
            opacity="0.40"
            style="filter:url(#logo-shadow-filter)"
        >
            <path d="\(appLabelPath)" fill="black" />
        </g>
        <path id="fork-label" d="\(appLabelPath)" fill="white" />
        <defs>
    """
guard let defsRange = appIcon.range(of: "    <defs>") else { fatalError("No <defs> in \(appIconSource)") }
appIcon.replaceSubrange(defsRange, with: appLabelMarkup)
write(appIcon, to: appIconTarget)
for size in [16, 24, 32, 48, 64, 128, 256, 512, 1024] {
    renderPNG(appIconTarget, "theme/colored/\(size)-NextcloudFork-macOS-icon.png", height: size, width: size)
}

// MARK: - Menu bar state icons

let labelColors = ["colored": "#FFF", "black": "#000", "white": "#FFF"]
let stateSize: CGFloat = 16
let glyphAreaHeight: CGFloat = 10.6
let labelFontSize: CGFloat = 6.9
let labelBaseline: CGFloat = 15.7

for (flavor, color) in labelColors.sorted(by: { $0.key < $1.key }) {
    let sourceDir = "theme/\(flavor)/nextcloud"
    let targetDir = "theme/\(flavor)/nextcloudfork"
    let states = try! FileManager.default.contentsOfDirectory(atPath: sourceDir)
        .filter { $0.hasPrefix("state-") && $0.hasSuffix(".svg") }
        .sorted()
    for state in states {
        let source = read("\(sourceDir)/\(state)")
        guard let rootEnd = source.range(of: ">", range: source.range(of: "<svg")!.upperBound..<source.endIndex),
              let closing = source.range(of: "</svg>", options: .backwards)
        else { fatalError("Unexpected SVG in \(sourceDir)/\(state)") }
        let root = String(source[source.range(of: "<svg")!.lowerBound..<rootEnd.upperBound])
        let body = String(source[rootEnd.upperBound..<closing.lowerBound])
        // Most glyphs are 16x16, the warning triangle is 18x16.
        let viewBox = root.components(separatedBy: "viewBox=\"").dropFirst().first?
            .components(separatedBy: "\"").first?
            .split(separator: " ").compactMap { Double($0) } ?? []
        let glyphWidth = viewBox.count == 4 ? CGFloat(viewBox[2]) : 16
        let glyphHeight = viewBox.count == 4 ? CGFloat(viewBox[3]) : 16
        let bounds = paintedBounds(ofSVG: "\(sourceDir)/\(state)", viewBoxWidth: glyphWidth, viewBoxHeight: glyphHeight)
        let scale = min(glyphAreaHeight / bounds.height, stateSize / bounds.width)
        let glyphX = (stateSize - bounds.width * scale) / 2 - bounds.minX * scale
        let glyphY = (glyphAreaHeight - bounds.height * scale) / 2 - bounds.minY * scale
        let textWidth = labelPath(label, fontSize: labelFontSize, weight: .bold, originX: 0, baselineY: 0).width
        let text = labelPath(label, fontSize: labelFontSize, weight: .bold,
                             originX: (stateSize - textWidth) / 2, baselineY: labelBaseline)
        let size = Int(stateSize)
        let svg = """
            <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" \
            width="\(size)" height="\(size)" viewBox="0 0 \(size) \(size)">\
            <g transform="translate(\(String(format: "%.3f", glyphX)) \(String(format: "%.3f", glyphY))) scale(\(String(format: "%.4f", scale)))">\(body)</g>\
            <path fill="\(color)" d="\(text.path)"/></svg>

            """
        let target = "\(targetDir)/\(state)"
        write(svg, to: target)
        let name = (state as NSString).deletingPathExtension
        for pixels in [16, 32, 64, 128, 256] {
            renderPNG(target, "\(targetDir)/\(name)-\(pixels).png", height: pixels, width: pixels)
        }
    }
}
// MARK: - Finder "Locations" symbol

let assets = "shell_integration/MacOSX/NextcloudIntegration/FileProviderExt/Assets.xcassets"
let symbolSource = "\(assets)/FileProviderDomainSymbol.symbolset"
let symbolTarget = "\(assets)/FileProviderForkSymbol.symbolset"
// Glyph coordinates are relative to the baseline (y = 0, up is negative); the template's margin box spans
// roughly y -95...24. The logo shrinks into the upper part, the label sits on the baseline.
let symbolLogoScale: CGFloat = 0.9
let symbolLogoTop: CGFloat = -90
let symbolLabelWidth: CGFloat = 82

/// Applies `transform` to every coordinate pair of a path that uses only absolute M, C and Z commands.
func transformPath(_ d: String, _ transform: (CGPoint) -> CGPoint) -> String {
    var result = ""
    var number = ""
    var numbers: [CGFloat] = []
    func flushNumber() {
        if let value = Double(number) { numbers.append(CGFloat(value)) }
        number = ""
    }
    func flushPairs() {
        flushNumber()
        precondition(numbers.count % 2 == 0, "Odd number of coordinates in symbol path")
        for index in stride(from: 0, to: numbers.count, by: 2) {
            let point = transform(CGPoint(x: numbers[index], y: numbers[index + 1]))
            result += (index == 0 ? "" : " ") + String(format: "%.4f %.4f", point.x, point.y)
        }
        numbers = []
    }
    for character in d {
        switch character {
        case "M", "C", "Z":
            flushPairs()
            result.append(character)
        case "L", "H", "V", "Q", "S", "T", "A", "m", "c", "z", "l", "h", "v", "q", "s", "t", "a":
            fatalError("Unsupported command \(character) in symbol path")
        case "-":
            flushNumber()
            number.append(character)
        case " ", ",":
            flushNumber()
        default:
            number.append(character)
        }
    }
    flushPairs()
    return result
}

/// The bounding box of the coordinates of an M/C/Z path (control points included, close enough here).
func pathBounds(_ d: String) -> CGRect {
    var minX = CGFloat.infinity, minY = CGFloat.infinity, maxX = -CGFloat.infinity, maxY = -CGFloat.infinity
    _ = transformPath(d) { point in
        minX = min(minX, point.x); maxX = max(maxX, point.x); minY = min(minY, point.y); maxY = max(maxY, point.y)
        return point
    }
    return CGRect(x: minX, y: minY, width: maxX - minX, height: maxY - minY)
}

var symbol = read("\(symbolSource)/FileProviderDomainSymbol.svg")
let referenceWidth = labelPath(label, fontSize: 100, weight: .bold, originX: 0, baselineY: 0).width
let symbolFontSize = 100 * symbolLabelWidth / referenceWidth
for weight in ["Ultralight-S", "Regular-S", "Black-S"] {
    guard let group = symbol.range(of: "<g id=\"\(weight)\""),
          let dStart = symbol.range(of: " d=\"", range: group.upperBound..<symbol.endIndex),
          let dEnd = symbol.range(of: "\"", range: dStart.upperBound..<symbol.endIndex)
    else { fatalError("No \(weight) glyph in \(symbolSource)") }
    let logo = String(symbol[dStart.upperBound..<dEnd.lowerBound])
    let bounds = pathBounds(logo)
    let centerX = bounds.midX
    let scaledLogo = transformPath(logo) { point in
        CGPoint(x: centerX + (point.x - centerX) * symbolLogoScale,
                y: symbolLogoTop + (point.y - bounds.minY) * symbolLogoScale)
    }
    let text = labelPath(label, fontSize: symbolFontSize, weight: .bold,
                         originX: centerX - symbolLabelWidth / 2, baselineY: 0)
    // CoreText outlines may contain line and quadratic segments; the template accepts any path data.
    symbol.replaceSubrange(dStart.upperBound..<dEnd.lowerBound, with: scaledLogo + text.path)
}
write(symbol, to: "\(symbolTarget)/FileProviderForkSymbol.svg")
write(read("\(symbolSource)/Contents.json").replacingOccurrences(of: "FileProviderDomainSymbol.svg",
                                                                  with: "FileProviderForkSymbol.svg"),
      to: "\(symbolTarget)/Contents.json")

print("Fork icons written.")
