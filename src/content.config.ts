import {
  baseFrontmatter,
  pageFrontmatter,
  postLoaderOptions,
} from '@tsche/astro-blog-theme/content';
import { defineCollection } from 'astro:content';
import { glob } from 'astro/loaders';

const posts = defineCollection({
  loader: glob(postLoaderOptions('./content')),
  schema: baseFrontmatter,
});

const pages = defineCollection({
  loader: glob({ base: './content/pages', pattern: '**/*.{md,mdx}' }),
  schema: pageFrontmatter,
});

export const collections = { posts, pages };
