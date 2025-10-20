#!/usr/bin/env python3
"""
Text Mosaic Generator - Core image processing functionality
Creates text mosaics using images arranged to form text characters
"""

import os
import sys
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import hashlib
import json
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from typing import List, Tuple, Dict, Optional
import logging
import multiprocessing
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class MosaicGenerator:
    """Main class for generating text mosaics from images"""
    
    def __init__(self, max_workers: Optional[int] = None):
        """
        Initialize the mosaic generator
        
        Args:
            max_workers: Maximum number of worker processes (defaults to CPU count)
        """
        self.max_workers = max_workers or min(multiprocessing.cpu_count(), 4)
        self.image_cache = {}
        self.font_cache = {}
        
    def load_images_from_directory(self, directory: Path, extensions: Tuple[str, ...] = ('.jpg', '.jpeg', '.png', '.webp')) -> List[Path]:
        """
        Load all valid image files from a directory
        
        Args:
            directory: Directory containing images
            extensions: Valid file extensions
            
        Returns:
            List of image file paths
        """
        images = []
        if not directory.exists():
            logger.warning(f"Directory {directory} does not exist")
            return images
            
        for ext in extensions:
            images.extend(directory.glob(f"*{ext}"))
            images.extend(directory.glob(f"*{ext.upper()}"))
            
        logger.info(f"Found {len(images)} images in {directory}")
        return images
    
    def calculate_image_hash(self, image_path: Path) -> str:
        """
        Calculate perceptual hash of an image for deduplication
        
        Args:
            image_path: Path to image file
            
        Returns:
            Hash string
        """
        try:
            img = cv2.imread(str(image_path))
            if img is None:
                return ""
            
            # Resize to 8x8 and convert to grayscale for perceptual hashing
            img_small = cv2.resize(img, (8, 8))
            img_gray = cv2.cvtColor(img_small, cv2.COLOR_BGR2GRAY)
            
            # Calculate average
            avg = img_gray.mean()
            
            # Create hash based on pixels above/below average
            hash_bits = (img_gray > avg).flatten()
            hash_str = ''.join('1' if bit else '0' for bit in hash_bits)
            
            return hash_str
        except Exception as e:
            logger.warning(f"Failed to calculate hash for {image_path}: {e}")
            return ""
    
    def deduplicate_images(self, image_paths: List[Path]) -> List[Path]:
        """
        Remove duplicate images based on perceptual hashing
        
        Args:
            image_paths: List of image paths
            
        Returns:
            List of unique image paths
        """
        unique_images = []
        seen_hashes = set()
        
        logger.info("Deduplicating images...")
        
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            hash_results = list(executor.map(self.calculate_image_hash, image_paths))
            
        for path, img_hash in zip(image_paths, hash_results):
            if img_hash and img_hash not in seen_hashes:
                seen_hashes.add(img_hash)
                unique_images.append(path)
                
        logger.info(f"Removed {len(image_paths) - len(unique_images)} duplicate images")
        return unique_images
    
    def create_circular_image(self, image_path: Path, size: int = 100) -> Optional[Image.Image]:
        """
        Create a circular version of an image
        
        Args:
            image_path: Path to image file
            size: Target size for the circular image
            
        Returns:
            PIL Image object or None if failed
        """
        try:
            # Load and resize image
            img = Image.open(image_path).convert('RGBA')
            img = img.resize((size, size), Image.Resampling.LANCZOS)
            
            # Create circular mask
            mask = Image.new('L', (size, size), 0)
            draw = ImageDraw.Draw(mask)
            draw.ellipse((0, 0, size, size), fill=255)
            
            # Apply mask
            result = Image.new('RGBA', (size, size))
            result.paste(img, (0, 0))
            result.putalpha(mask)
            
            return result
            
        except Exception as e:
            logger.warning(f"Failed to process image {image_path}: {e}")
            return None
    
    def create_text_mask(self, text: str, font_path: Optional[str] = None, 
                        font_size: int = 200, canvas_size: Tuple[int, int] = (1200, 400)) -> np.ndarray:
        """
        Create a binary mask from text
        
        Args:
            text: Text to render
            font_path: Path to font file (optional)
            font_size: Size of font
            canvas_size: Size of canvas (width, height)
            
        Returns:
            Binary mask as numpy array
        """
        try:
            # Create image
            img = Image.new('L', canvas_size, 0)
            draw = ImageDraw.Draw(img)
            
            # Load font
            font_key = (font_path, font_size)
            if font_key not in self.font_cache:
                try:
                    if font_path and os.path.exists(font_path):
                        font = ImageFont.truetype(font_path, font_size)
                    else:
                        # Try to use default system font
                        font = ImageFont.load_default()
                        # Scale default font if needed
                        if hasattr(font, 'font_size') and font_size != font.font_size:
                            try:
                                font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
                            except:
                                pass
                    self.font_cache[font_key] = font
                except Exception as e:
                    logger.warning(f"Font loading failed: {e}, using default")
                    self.font_cache[font_key] = ImageFont.load_default()
            
            font = self.font_cache[font_key]
            
            # Get text dimensions
            bbox = draw.textbbox((0, 0), text, font=font)
            text_width = bbox[2] - bbox[0]
            text_height = bbox[3] - bbox[1]
            
            # Center text
            x = (canvas_size[0] - text_width) // 2
            y = (canvas_size[1] - text_height) // 2
            
            # Draw text
            draw.text((x, y), text, fill=255, font=font)
            
            # Convert to numpy array
            mask = np.array(img)
            return mask > 127  # Binary threshold
            
        except Exception as e:
            logger.error(f"Failed to create text mask: {e}")
            return np.zeros(canvas_size[::-1], dtype=bool)  # Return empty mask
    
    def place_images_on_mask(self, mask: np.ndarray, image_paths: List[Path], 
                           circle_size: int = 30, overlap_factor: float = 0.8) -> Image.Image:
        """
        Place circular images on the text mask
        
        Args:
            mask: Binary mask where images should be placed
            image_paths: List of image paths to use
            circle_size: Size of each circular image
            overlap_factor: How much images can overlap (0.0 = no overlap, 1.0 = full overlap)
            
        Returns:
            Final mosaic image
        """
        if not image_paths:
            logger.error("No images provided")
            return Image.new('RGBA', (mask.shape[1], mask.shape[0]), (255, 255, 255, 255))
        
        # Calculate step size based on overlap
        step_size = int(circle_size * overlap_factor)
        if step_size <= 0:
            step_size = 1
            
        # Create result image
        result = Image.new('RGBA', (mask.shape[1], mask.shape[0]), (255, 255, 255, 255))
        
        # Find positions where mask is True
        positions = []
        for y in range(0, mask.shape[0] - circle_size, step_size):
            for x in range(0, mask.shape[1] - circle_size, step_size):
                # Check if this area has enough mask coverage
                mask_region = mask[y:y+circle_size, x:x+circle_size]
                coverage = np.sum(mask_region) / (circle_size * circle_size)
                if coverage > 0.3:  # At least 30% coverage
                    positions.append((x, y))
        
        if not positions:
            logger.warning("No valid positions found for image placement")
            return result
        
        logger.info(f"Placing {len(positions)} images")
        
        # Process images in parallel
        def process_image_batch(batch_info):
            batch_positions, batch_paths = batch_info
            batch_images = []
            
            for (x, y), img_path in zip(batch_positions, batch_paths):
                circular_img = self.create_circular_image(img_path, circle_size)
                if circular_img:
                    batch_images.append((x, y, circular_img))
            
            return batch_images
        
        # Create batches for parallel processing
        batch_size = max(1, len(positions) // self.max_workers)
        batches = []
        
        for i in range(0, len(positions), batch_size):
            batch_positions = positions[i:i+batch_size]
            # Cycle through available images
            batch_paths = [image_paths[j % len(image_paths)] for j in range(i, i+len(batch_positions))]
            batches.append((batch_positions, batch_paths))
        
        # Process batches in parallel
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            batch_results = list(executor.map(process_image_batch, batches))
        
        # Paste all images onto result
        for batch_images in batch_results:
            for x, y, circular_img in batch_images:
                result.paste(circular_img, (x, y), circular_img)
        
        return result
    
    def generate_mosaic(self, text: str, image_directory: Path, 
                       font_path: Optional[str] = None, font_size: int = 200,
                       canvas_size: Tuple[int, int] = (1200, 400),
                       circle_size: int = 30, overlap_factor: float = 0.8,
                       deduplicate: bool = True) -> Optional[Image.Image]:
        """
        Generate a complete text mosaic
        
        Args:
            text: Text to create mosaic for
            image_directory: Directory containing source images
            font_path: Path to font file
            font_size: Font size for text
            canvas_size: Canvas dimensions
            circle_size: Size of circular images
            overlap_factor: Image overlap factor
            deduplicate: Whether to remove duplicate images
            
        Returns:
            Generated mosaic image or None if failed
        """
        try:
            logger.info(f"Starting mosaic generation for text: '{text}'")
            
            # Load images
            image_paths = self.load_images_from_directory(image_directory)
            if not image_paths:
                logger.error("No images found in directory")
                return None
            
            # Deduplicate if requested
            if deduplicate:
                image_paths = self.deduplicate_images(image_paths)
                if not image_paths:
                    logger.error("No unique images found after deduplication")
                    return None
            
            # Create text mask
            logger.info("Creating text mask...")
            mask = self.create_text_mask(text, font_path, font_size, canvas_size)
            
            # Generate mosaic
            logger.info("Placing images on mask...")
            mosaic = self.place_images_on_mask(mask, image_paths, circle_size, overlap_factor)
            
            logger.info("Mosaic generation completed")
            return mosaic
            
        except Exception as e:
            logger.error(f"Failed to generate mosaic: {e}")
            return None

def main():
    """Example usage of the MosaicGenerator"""
    if len(sys.argv) != 4:
        print("Usage: python mosaic_generator.py <text> <image_directory> <output_file>")
        sys.exit(1)
    
    text = sys.argv[1]
    image_dir = Path(sys.argv[2])
    output_file = sys.argv[3]
    
    generator = MosaicGenerator()
    mosaic = generator.generate_mosaic(text, image_dir)
    
    if mosaic:
        mosaic.save(output_file, "PNG")
        print(f"Mosaic saved to {output_file}")
    else:
        print("Failed to generate mosaic")
        sys.exit(1)

if __name__ == "__main__":
    main()