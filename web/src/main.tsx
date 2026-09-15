import '@mantine/core/styles.css';
import '@mantine/notifications/styles.css';
import './app.css';

import { createTheme, MantineProvider } from '@mantine/core';
import { Notifications } from '@mantine/notifications';
import { QueryClientProvider } from '@tanstack/react-query';
import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { queryClient } from './api/queryClient';
import { useAnalysisSettings } from './analysis/settingsStore';
import { App } from './App';
import { useFeatureMap } from './featureMaps/featureMapStore';
import { useResults } from './results/resultsStore';
import { useRois } from './rois/roiStore';
import { useViewer } from './stores/viewerStore';

// End-to-end tests (e2e/) open the app with ?testHooks to read viewport, ROI and result state
if (new URLSearchParams(window.location.search).has('testHooks')) {
  Object.assign(window, { __glcm: { viewer: useViewer, rois: useRois, results: useResults, settings: useAnalysisSettings, featureMap: useFeatureMap } });
}

const theme = createTheme({
  primaryColor: 'blue',
  fontFamilyMonospace: 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
});

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <MantineProvider theme={theme} defaultColorScheme="dark">
      <Notifications position="bottom-right" limit={4} />
      <QueryClientProvider client={queryClient}>
        <App />
      </QueryClientProvider>
    </MantineProvider>
  </StrictMode>,
);
